#include "CudaBufferStorage.h"

#include <cuda.h>

#include "Execution.h"
#include "ExecutionContext.h"
#include "Utils/CheckedSize.h"

namespace libcbct {

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
        checkDriver(cuInit(0), "cuInit");

        auto state = std::shared_ptr<CudaDeviceState>(new CudaDeviceState());

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
    static T *pointer(const TensorView<T, Rank> &view, const ExecutionContext &expectedContext) {
        if (expectedContext.backend() != Backend::Cuda || view.context() != expectedContext) {
            throw InvalidArgument("CUDA execution context mismatch");
        }

        auto *storage = dynamic_cast<detail::CudaBufferStorage *>(view.storage_.get());

        if (!storage || storage->context() != expectedContext) {
            throw InvalidArgument("Buffer is not CUDA storage");
        }

        std::size_t count = 1;
        for (auto extent : view.shape()) {
            count = detail::checkedProduct(count, extent);
        }

        const auto bytes = detail::checkedProduct(count, sizeof(T));
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

namespace detail {

class CudaContextScope final {
public:
    explicit CudaContextScope(CUcontext context) {
        if (!context) {
            throw InvalidArgument("CUDA context must not be null");
        }

        checkDriver(cuCtxPushCurrent(context), "cuCtxPushCurrent");
    }

    ~CudaContextScope() {
        CUcontext popped = nullptr;
        cuCtxPopCurrent(&popped);
    }

    CudaContextScope(const CudaContextScope &) = delete;
    CudaContextScope &operator=(const CudaContextScope &) = delete;
};

CudaBufferStorage::CudaBufferStorage(ExecutionContext context, std::shared_ptr<CudaDeviceState> device,
                                     std::size_t sizeBytes)
    : BufferStorage(std::move(context), sizeBytes)
    , device_(std::move(device)) {
    if (this->context().backend() != Backend::Cuda) {
        throw InvalidArgument("CUDA storage requires a CUDA execution context");
    }

    if (!device_ || !device_->context()) {
        throw InvalidArgument("CUDA storage requires a valid device state");
    }

    if (!sizeBytes || sizeBytes > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
        throw InvalidArgument("CUDA storage size must be positive and addressable");
    }

    CudaContextScope scope(device_->context());
    checkDriver(cuMemAlloc(&address_, sizeBytes), "cuMemAlloc");
}

CudaBufferStorage::~CudaBufferStorage() noexcept {
    if (address_ && cuCtxPushCurrent(device_->context()) == CUDA_SUCCESS) {
        cuMemFree(address_);
        CUcontext popped = nullptr;
        cuCtxPopCurrent(&popped);
    }
}

}  // namespace detail

}  // namespace libcbct
