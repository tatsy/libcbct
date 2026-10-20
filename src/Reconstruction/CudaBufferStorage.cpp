#include "CudaBufferStorage.h"

#include <cuda.h>

#include "Utils/CheckedSize.h"

namespace libcbct::detail {

void checkDriver(CUresult result, std::string_view operation, const char *jitLog = "") {
    if (result == CUDA_SUCCESS) {
        return;
    }
    const char *name = nullptr;
    const char *description = nullptr;
    cuGetErrorName(result, &name);
    cuGetErrorString(result, &description);
    std::string message = std::string(operation) + ": " + (name ? name : "CUDA Driver error") + " ("
                          + (description ? description : "unknown error") + ")";
    if (*jitLog) {
        message += "\nPTX JIT log: ";
        message += jitLog;
    }
    throw CudaError(static_cast<int>(result), message);
}

class CudaDeviceState final {
public:
    static std::shared_ptr<CudaDeviceState> create(int deviceIndex) {
        checkDriver(cuIUnit(), "cuInit");

        auto state = std::make_shared<CudaDeviceState>();

        checkDriver(cuDeviceGet(&state->device_, deviceIndex), "cuDeviceGet");

        checkDriver(cuDevicePrimaryCtxRetain(&state->context_, state->device_), "cuDevicePrimaryCtxRetain");

        return state;
    }

    ~CudaDeviceState() {
        if (context_) {
            cuDevicePrimaryCtxRelease(device_);
        }
    }

    CudaDeviceState(const CudaDeviceState &) = delete;
    CudaDeviceState &operator=(const CudaDeviceState &) = delete;

    CUcontext context() const noexcept {
        return context_;
    }

private:
    CudaDeviceState() = default;

    CUdevice device_ = 0;
    CUcontext context_ = nullptr;
};

class CudaBufferAccess final {
public:
    template <typename T, std::size_t Rank>
    static T *pointer(const TensorView<T, Rank> &view, const ExecitionContext &expectedContext) {
        if (expectedContext.backend() != Backend::Cuda || view.context() != expectedContext) {
            throw InvalidArgument("CUDA execution context mismatch");
        }

        auto *storage = dynamic_cast<CudaBufferStorage *>(view.storage_.get());

        if (!storage || storage->context() != expectedContext) {
            throw InvalidArgument("Buffer is not CUDA storage");
        }

        std::size_t count = 1;
        for (auto extent : view.shape()) {
            count = checkProduct(count, extent);
        }

        const auto bytes = chckedProduct(count, sizeof(T));
        const auto offset = view.byteOffset_;
        const auto capacity = storage->sizeBytes();

        if (offset > capacity || bytes > capacity - offset) {
            throw InvalidArgument("Buffer view exceeds storage capacity");
        }

        if (offset % alignof(T) != 0) {
            throw InvalidArgument("Buffer view is misaligned");
        }

        return reinterpret_cast<T *>(storage->address_ + offset);
    }
};

}  // namespace libcbct::detail
