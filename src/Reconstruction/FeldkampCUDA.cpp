#include "FeldkampCUDA.h"

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "Common/Constants.h"
#include "Common/ProgressBar.h"
#include "Reconstruction/FilterUtils.h"
#include "Reconstruction/Slang/FdkCudaParameters.h"
#include "Utils/CudaPtxModule.h"
#include "Utils/DeviceBuffer.h"

namespace libcbct {

namespace {

class FftPlan {
public:
    FftPlan(int length, cufftType type, int batch) {
        CUFFT_CHECK(cufftCreate(&value));
        try {
            std::size_t workspaceBytes = 0;
            CUFFT_CHECK(cufftMakePlan1d(value, length, type, batch, &workspaceBytes));
        } catch (...) {
            cufftDestroy(value);
            throw;
        }
    }
    ~FftPlan() {
        cufftDestroy(value);
    }
    FftPlan(const FftPlan &) = delete;
    FftPlan &operator=(const FftPlan &) = delete;
    cufftHandle value = 0;
};

class Stream {
public:
    Stream() {
        CUDA_CHECK(cudaStreamCreateWithFlags(&value, cudaStreamNonBlocking));
    }
    ~Stream() {
        // This object is destroyed before buffers, FFT plans, and PTX modules, also on exceptions.
        cudaStreamSynchronize(value);
        cudaStreamDestroy(value);
    }
    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;
    cudaStream_t value = nullptr;
};

std::size_t checkedProduct(std::size_t a, std::size_t b) {
    if (b && a > std::numeric_limits<std::size_t>::max() / b) {
        throw libcbct::InvalidArgument("FDK allocation size overflow");
    }
    return a * b;
}

bool positiveFinite(float value) {
    return std::isfinite(value) && value > 0.0f;
}

libcbct::CudaPtxKernel loadKernel(const std::filesystem::path &directory, const std::string &name) {
    libcbct::detail::verifyFdkCudaLayoutFile(directory / (name + ".ptx.json"), name);
    // The kernel keeps both the module and its primary context alive.
    return libcbct::CudaPtxModule::fromFile(directory / (name + ".ptx")).getKernel(name);
}

template <typename Parameters>
void launch(const libcbct::CudaPtxKernel &kernel, dim3 grid, Parameters &params, cudaStream_t stream) {
    void *arguments[] = { &params };
    kernel.launch(grid, dim3(16, 16, 1), arguments, stream);
}

}  // namespace

FeldkampCUDA::FeldkampCUDA(RampFilter filter)
    : FeldkampCUDA(std::filesystem::path(LIBCBCT_SLANG_KERNEL_DIR), filter) {
}

FeldkampCUDA::FeldkampCUDA(std::filesystem::path kernelDirectory, RampFilter filter)
    : kernelDirectory_(std::move(kernelDirectory))
    , filter_(filter) {
}

VolumeF32 FeldkampCUDA::reconstruct(const VolumeF32 &sinogram, const Geometry &geometry) const {
    using namespace libcbct::detail;
    const std::size_t detWidth = sinogram.size<0>();
    const std::size_t detHeight = sinogram.size<1>();
    const std::size_t nProj = sinogram.size<2>();
    const vec3i volSize = geometry.volSize;
    if (detWidth < 2 || detHeight < 2 || !nProj || geometry.detSize.x < 2 || geometry.detSize.y < 2 || volSize.x <= 0
        || volSize.y <= 0 || volSize.z <= 0 || !positiveFinite(geometry.pixSize.x)
        || !positiveFinite(geometry.pixSize.y) || !positiveFinite(geometry.sod) || !positiveFinite(geometry.sdd)) {
        throw libcbct::InvalidArgument(
            "FDK requires detectors of at least 2x2, projections, and positive finite geometry");
    }
    if (filter_ != RampFilter::RamLak && filter_ != RampFilter::SheppLogan) {
        throw libcbct::InvalidArgument("Unknown FDK ramp filter");
    }
    const std::size_t nPad = paddedFFTLength(detWidth);
    const std::size_t nFreq = nPad / 2 + 1;
    // Detector kernels and cuFFT use signed 32-bit dimensions and row indices.
    if (checkedProduct(detHeight, nPad) > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw libcbct::InvalidArgument("FDK detector/FFT dimensions exceed 32-bit indexing");
    }
    const std::size_t nPixels = detWidth * detHeight;
    const std::size_t nVoxels = checkedProduct(checkedProduct(volSize.x, volSize.y), volSize.z);
    const std::size_t volumeBytes = checkedProduct(nVoxels, sizeof(float));

    const auto weightKernel = loadKernel(kernelDirectory_, "weightAndPadKernel");
    const auto filterKernel = loadKernel(kernelDirectory_, "applyFilterKernel");
    const auto backKernel = loadKernel(kernelDirectory_, "backProjectionKernel");
    VolumeF32 tomogram(volSize.x, volSize.y, volSize.z);

    const double dp = geometry.pixSize.x * geometry.sod / geometry.sdd;
    std::vector<float> response = makeRampResponse(nPad, dp, filter_);
    for (float &h : response) h /= static_cast<float>(nPad);
    const std::vector<float> weights = makeCosineWeights(geometry, detWidth, detHeight);

    DeviceBuffer<float> devProj(nPixels), devWeights(nPixels), devPadded(detHeight * nPad);
    DeviceBuffer<cufftComplex> devSpec(detHeight * nFreq);
    DeviceBuffer<float> devResponse(nFreq), devFiltered(nPixels), devVolume(nVoxels);
    static_assert(sizeof(cufftComplex) == 2 * sizeof(float));
    FftPlan forward(static_cast<int>(nPad), CUFFT_R2C, static_cast<int>(detHeight));
    FftPlan inverse(static_cast<int>(nPad), CUFFT_C2R, static_cast<int>(detHeight));
    Stream stream;
    CUFFT_CHECK(cufftSetStream(forward.value, stream.value));
    CUFFT_CHECK(cufftSetStream(inverse.value, stream.value));

    CUDA_CHECK(cudaMemsetAsync(devVolume.ptr(), 0, volumeBytes, stream.value));
    CUDA_CHECK(cudaMemcpyAsync(devWeights.ptr(), weights.data(), nPixels * sizeof(float), cudaMemcpyHostToDevice,
                               stream.value));
    CUDA_CHECK(cudaMemcpyAsync(devResponse.ptr(), response.data(), nFreq * sizeof(float), cudaMemcpyHostToDevice,
                               stream.value));

    WeightAndPadArgs weightParams{
        { devProj.ptr(), nPixels }, { devWeights.ptr(), nPixels }, { devPadded.ptr(), detHeight * nPad },
        static_cast<int>(detWidth), static_cast<int>(detHeight),   static_cast<int>(nPad)
    };
    FilterArgs filterParams{ { devSpec.ptr(), detHeight * nFreq },
                             { devResponse.ptr(), nFreq },
                             static_cast<int>(nFreq),
                             static_cast<int>(detHeight) };
    const FdkCudaGeometry gpuGeometry{ { geometry.detSize.x, geometry.detSize.y },
                                       { geometry.pixSize.x, geometry.pixSize.y },
                                       { volSize.x, volSize.y, volSize.z },
                                       geometry.sod,
                                       geometry.sdd };
    BackProjectionArgs backParams{ { devFiltered.ptr(), nPixels },
                                   { devVolume.ptr(), nVoxels },
                                   static_cast<int>(detWidth),
                                   static_cast<int>(detHeight),
                                   gpuGeometry,
                                   0.0f,
                                   static_cast<float>(libcbct::kPi) / nProj };
    const dim3 gridPad((nPad + 15) / 16, (detHeight + 15) / 16, 1);
    const dim3 gridFreq((nFreq + 15) / 16, (detHeight + 15) / 16, 1);
    const dim3 gridVolume((static_cast<unsigned int>(volSize.x) + 15) / 16,
                          (static_cast<unsigned int>(volSize.y) + 15) / 16, volSize.z);

    ProgressBar pbar(nProj);
    pbar.setDescription("RECON: ");
    for (std::size_t i = 0; i < nProj; ++i) {
        CUDA_CHECK(cudaMemcpyAsync(devProj.ptr(), sinogram.ptr() + nPixels * i, nPixels * sizeof(float),
                                   cudaMemcpyHostToDevice, stream.value));
        launch(weightKernel, gridPad, weightParams, stream.value);
        CUFFT_CHECK(cufftExecR2C(forward.value, devPadded.ptr(), devSpec.ptr()));
        launch(filterKernel, gridFreq, filterParams, stream.value);
        CUFFT_CHECK(cufftExecC2R(inverse.value, devSpec.ptr(), devPadded.ptr()));
        CUDA_CHECK(cudaMemcpy2DAsync(devFiltered.ptr(), detWidth * sizeof(float), devPadded.ptr(), nPad * sizeof(float),
                                     detWidth * sizeof(float), detHeight, cudaMemcpyDeviceToDevice, stream.value));
        backParams.theta = static_cast<float>(libcbct::kTwoPi) * i / nProj;
        launch(backKernel, gridVolume, backParams, stream.value);
        // Preserve projection ordering and report asynchronous execution errors here.
        CUDA_CHECK(cudaStreamSynchronize(stream.value));
        pbar.step();
    }
    CUDA_CHECK(cudaMemcpyAsync(tomogram.ptr(), devVolume.ptr(), volumeBytes, cudaMemcpyDeviceToHost, stream.value));
    CUDA_CHECK(cudaStreamSynchronize(stream.value));
    return tomogram;
}

}  // namespace libcbct
