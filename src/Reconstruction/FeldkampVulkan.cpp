#include "FeldkampVulkan.h"

#include <cmath>
#include <limits>
#include <vector>

#include "Common/Constants.h"
#include "Common/Logging.h"
#include "Common/ProgressBar.h"
#include "FilterUtils.h"
#include "Utils/SlangSpirvKernel.h"
#include "Utils/VulkanFft.h"

namespace {

std::size_t checkedProduct(std::size_t a, std::size_t b) {
    if (b && a > std::numeric_limits<std::size_t>::max() / b)
        throw libcbct::InvalidArgument("Vulkan FDK allocation size overflow");
    return a * b;
}

bool positiveFinite(float x) {
    return std::isfinite(x) && x > 0.0f;
}

std::array<std::uint32_t, 3> groups(int width, int height, int depth = 1) {
    return { (static_cast<std::uint32_t>(width) + 15) / 16, (static_cast<std::uint32_t>(height) + 15) / 16,
             static_cast<std::uint32_t>(depth) };
}

}  // namespace

VolumeF32 FeldkampVulkan::reconstruct(const VolumeF32 &sinogram, const Geometry &geometry) const {
    using namespace libcbct;
    const auto width = sinogram.size<0>(), height = sinogram.size<1>(), projections = sinogram.size<2>();
    const auto volumeSize = geometry.volSize;
    if (width < 2 || height < 2 || !projections || geometry.detSize.x < 2 || geometry.detSize.y < 2 || volumeSize.x <= 0
        || volumeSize.y <= 0 || volumeSize.z <= 0 || !positiveFinite(geometry.pixSize.x)
        || !positiveFinite(geometry.pixSize.y) || !positiveFinite(geometry.sod) || !positiveFinite(geometry.sdd)) {
        throw InvalidArgument("FDK requires detectors of at least 2x2, projections, and positive finite geometry");
    }
    if (filter_ != RampFilter::RamLak && filter_ != RampFilter::SheppLogan)
        throw InvalidArgument("Unknown FDK ramp filter");
    const auto length = paddedFFTLength(width);
    const auto paddedCount = checkedProduct(length, height);
    const auto pixels = checkedProduct(width, height);
    const auto voxels = checkedProduct(checkedProduct(volumeSize.x, volumeSize.y), volumeSize.z);
    if (paddedCount > static_cast<std::size_t>(std::numeric_limits<int>::max())
        || voxels > std::numeric_limits<std::uint32_t>::max()) {
        throw InvalidArgument("Vulkan FDK dimensions exceed 32-bit storage-buffer indexing");
    }
    const auto pixelBytes = checkedProduct(pixels, sizeof(float));
    const auto paddedBytes = checkedProduct(paddedCount, sizeof(float));
    const auto spectrumBytes = checkedProduct(paddedBytes, 2);
    const auto volumeBytes = checkedProduct(voxels, sizeof(float));
    VulkanComputeContext context(deviceIndex_);
    if (std::max(spectrumBytes, volumeBytes) > context.deviceInfo().maxStorageBufferBytes) {
        throw InvalidArgument("Vulkan FDK buffers exceed the device's maximum storage-buffer range");
    }
    LIBCBCT_INFO("Vulkan device #%d: %s", context.deviceInfo().index, context.deviceInfo().name.c_str());
    SlangSpirvKernel weight(context, kernelDirectory_, "weightAndPadKernel");
    SlangSpirvKernel filter(context, kernelDirectory_, "applyFilterKernel");
    SlangSpirvKernel crop(context, kernelDirectory_, "cropFilteredKernel");
    SlangSpirvKernel back(context, kernelDirectory_, "backProjectionKernel");
    VulkanFftPlan fft(context, kernelDirectory_, static_cast<int>(length), static_cast<int>(height));

    const auto projection = context.createBuffer(pixelBytes);
    const auto weights = context.createBuffer(pixelBytes);
    const auto padded = context.createBuffer(paddedBytes);
    const auto spectrum = context.createBuffer(spectrumBytes);
    const auto inverse = context.createBuffer(spectrumBytes);
    const auto filtered = context.createBuffer(pixelBytes);
    const auto volume = context.createBuffer(volumeBytes);
    const auto response = context.createBuffer(length * sizeof(float));
    const auto upload = context.createBuffer(pixelBytes, VulkanBufferMemory::Upload);
    const auto weightUpload = context.createBuffer(pixelBytes, VulkanBufferMemory::Upload);
    const auto responseUpload = context.createBuffer(length * sizeof(float), VulkanBufferMemory::Upload);
    const auto download = context.createBuffer(volumeBytes, VulkanBufferMemory::Readback);

    const auto cosineWeights = makeCosineWeights(geometry, width, height);
    const double dp = geometry.pixSize.x * geometry.sod / geometry.sdd;
    const auto halfResponse = makeRampResponse(length, dp, filter_);
    std::vector<float> fullResponse(length);
    for (std::size_t k = 0; k < length; ++k) {
        // Match cuFFT's unnormalized inverse and real-transform Hermitian spectrum.
        fullResponse[k] = halfResponse[std::min(k, length - k)] / static_cast<float>(length);
    }
    weightUpload.write(std::as_bytes(std::span(cosineWeights)));
    responseUpload.write(std::as_bytes(std::span(fullResponse)));
    {
        auto batch = context.createBatch();
        batch.copy(weightUpload, weights, pixelBytes);
        batch.copy(responseUpload, response, length * sizeof(float));
        batch.fill(volume);
        batch.submitAndWait();
    }

    const int w = static_cast<int>(width), h = static_cast<int>(height), n = static_cast<int>(length);
    weight.setBuffer("projection", projection);
    weight.setBuffer("weights", weights);
    weight.setBuffer("padded", padded);
    weight.setUniform("detWidth", w);
    weight.setUniform("detHeight", h);
    weight.setUniform("nPad", n);
    filter.setBuffer("spectrum", spectrum);
    filter.setBuffer("response", response);
    // Use the existing filter kernel on the full complex spectrum.
    filter.setUniform("nFreq", n);
    filter.setUniform("detHeight", h);
    crop.setBuffer("padded", inverse);
    crop.setBuffer("filtered", filtered);
    crop.setUniform("length", n);
    crop.setUniform("detWidth", w);
    crop.setUniform("detHeight", h);
    back.setBuffer("filtered", filtered);
    back.setBuffer("volume", volume);
    back.setUniform("detWidth", w);
    back.setUniform("detHeight", h);
    const std::int32_t detSize[] = { geometry.detSize.x, geometry.detSize.y };
    const float pixelSize[] = { geometry.pixSize.x, geometry.pixSize.y };
    const std::int32_t volSize[] = { volumeSize.x, volumeSize.y, volumeSize.z };
    back.setUniform("geometry.detSize", detSize);
    back.setUniform("geometry.pixSize", pixelSize);
    back.setUniform("geometry.volSize", volSize);
    back.setUniform("geometry.sod", geometry.sod);
    back.setUniform("geometry.sdd", geometry.sdd);
    back.setUniform("scale", static_cast<float>(libcbct::kPi) / projections);

    ProgressBar progress(projections);
    progress.setDescription("RECON: ");
    for (std::size_t i = 0; i < projections; ++i) {
        upload.write(std::as_bytes(std::span(sinogram.ptr() + pixels * i, pixels)));
        auto batch = context.createBatch();
        batch.copy(upload, projection, pixelBytes);
        weight.dispatch(batch, groups(n, h));
        fft.forward(batch, padded, spectrum);
        filter.dispatch(batch, groups(n, h));
        fft.inverse(batch, spectrum, inverse);
        crop.dispatch(batch, groups(w, h));
        back.setUniform("theta", static_cast<float>(libcbct::kTwoPi) * i / projections);
        // One invocation owns each voxel; ordered projections require no atomic add.
        back.dispatch(batch, groups(volumeSize.x, volumeSize.y, volumeSize.z));
        batch.submitAndWait();
        progress.step();
    }
    VolumeF32 tomogram(volumeSize.x, volumeSize.y, volumeSize.z);
    auto batch = context.createBatch();
    batch.copy(volume, download, volumeBytes);
    batch.submitAndWait();
    download.read(std::as_writable_bytes(std::span(tomogram.ptr(), voxels)));
    return tomogram;
}
