#include "VulkanFft.h"

#include <bit>
#include <limits>

#include "SlangSpirvKernel.h"

namespace libcbct {

struct VulkanFftPlan::State {
    int length;
    int rows;
    int bitCount;
    SlangSpirvKernel prepareReal;
    SlangSpirvKernel prepareComplex;
    SlangSpirvKernel stage;

    State(const VulkanComputeContext &context, const std::filesystem::path &directory, int n, int h)
        : length(n)
        , rows(h)
        , bitCount(std::countr_zero(static_cast<unsigned int>(n)))
        , prepareReal(context, directory, "fftPrepareRealKernel")
        , prepareComplex(context, directory, "fftPrepareComplexKernel")
        , stage(context, directory, "fftStageKernel") {
        for (auto *kernel : { &prepareReal, &prepareComplex }) {
            kernel->setUniform("length", length);
            kernel->setUniform("detHeight", rows);
            kernel->setUniform("bitCount", bitCount);
        }
        stage.setUniform("length", length);
        stage.setUniform("detHeight", rows);
    }

    void stages(VulkanComputeBatch &batch, const VulkanComputeBuffer &spectrum, bool inverse) {
        stage.setBuffer("spectrum", spectrum);
        stage.setUniform("inverse", inverse ? 1 : 0);
        for (int half = 1; half < length; half *= 2) {
            stage.setUniform("halfSpan", half);
            stage.dispatch(batch, { static_cast<std::uint32_t>((length / 2 + 15) / 16),
                                    static_cast<std::uint32_t>((rows + 15) / 16), 1 });
        }
    }
};

VulkanFftPlan::VulkanFftPlan(const VulkanComputeContext &context, const std::filesystem::path &directory, int length,
                             int rows) {
    if (length < 2 || !std::has_single_bit(static_cast<unsigned int>(length)) || rows <= 0
        || static_cast<std::uint64_t>(length) * rows > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw InvalidArgument("Vulkan FFT requires power-of-two rows and 32-bit indexing");
    }
    if (static_cast<std::uint64_t>(length) * rows * 2 * sizeof(float) > context.deviceInfo().maxStorageBufferBytes) {
        throw InvalidArgument("Vulkan FFT buffers exceed the device's storage-buffer range");
    }
    state_ = std::make_unique<State>(context, directory, length, rows);
}

VulkanFftPlan::~VulkanFftPlan() = default;

void VulkanFftPlan::forward(VulkanComputeBatch &batch, const VulkanComputeBuffer &input,
                            const VulkanComputeBuffer &spectrum) {
    const auto count = static_cast<std::size_t>(state_->length) * state_->rows;
    if (input.size() != count * sizeof(float) || spectrum.size() != count * 2 * sizeof(float)) {
        throw InvalidArgument("Vulkan FFT input/output sizes do not match the plan");
    }
    state_->prepareReal.setBuffer("padded", input);
    state_->prepareReal.setBuffer("spectrum", spectrum);
    state_->prepareReal.dispatch(batch, { static_cast<std::uint32_t>((state_->length + 15) / 16),
                                          static_cast<std::uint32_t>((state_->rows + 15) / 16), 1 });
    state_->stages(batch, spectrum, false);
}

void VulkanFftPlan::inverse(VulkanComputeBatch &batch, const VulkanComputeBuffer &spectrum,
                            const VulkanComputeBuffer &output) {
    const auto bytes = static_cast<std::size_t>(state_->length) * state_->rows * 2 * sizeof(float);
    if (spectrum.size() != bytes || output.size() != bytes || spectrum == output) {
        throw InvalidArgument("Vulkan FFT inverse requires distinct buffers of the planned size");
    }
    state_->prepareComplex.setBuffer("input", spectrum);
    state_->prepareComplex.setBuffer("output", output);
    state_->prepareComplex.dispatch(batch, { static_cast<std::uint32_t>((state_->length + 15) / 16),
                                             static_cast<std::uint32_t>((state_->rows + 15) / 16), 1 });
    state_->stages(batch, output, true);
}

}  // namespace libcbct
