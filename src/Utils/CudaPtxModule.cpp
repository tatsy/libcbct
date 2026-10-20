#include "CudaPtxModule.h"

#include <cuda.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

#include "Common/Exception.h"
#include "Utils/CudaUtils.h"

namespace libcbct {
namespace {

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

// Preserve the caller's current context, including when an operation throws.
class ContextScope {
public:
    explicit ContextScope(CUcontext context) {
        checkDriver(cuCtxPushCurrent(context), "cuCtxPushCurrent");
    }
    ~ContextScope() {
        CUcontext popped = nullptr;
        cuCtxPopCurrent(&popped);
    }
    ContextScope(const ContextScope &) = delete;
    ContextScope &operator=(const ContextScope &) = delete;
};

}  // namespace

namespace detail {

struct CudaPtxModuleState {
    CUdevice device = 0;
    CUcontext context = nullptr;
    CUmodule module = nullptr;

    ~CudaPtxModuleState() {
        if (module && cuCtxPushCurrent(context) == CUDA_SUCCESS) {
            cuModuleUnload(module);
            CUcontext popped = nullptr;
            cuCtxPopCurrent(&popped);
        }
        if (context) {
            cuDevicePrimaryCtxRelease(device);
        }
    }
};

}  // namespace detail

CudaPtxModule::CudaPtxModule(std::shared_ptr<detail::CudaPtxModuleState> state)
    : state_(std::move(state)) {
}

CudaPtxModule CudaPtxModule::fromFile(const std::filesystem::path &path, int device) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw IOError("Cannot open PTX file: " + path.string());
    }
    const std::string ptx((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.bad()) {
        throw IOError("Cannot read PTX file: " + path.string());
    }
    return fromPtx(ptx, device);
}

CudaPtxModule CudaPtxModule::fromPtx(std::string_view ptx, int device) {
    if (ptx.empty() || ptx.front() == '\0'
        || (ptx.find('\0') != std::string_view::npos && ptx.find('\0') != ptx.size() - 1)) {
        throw InvalidArgument("PTX source must be non-empty text without embedded null bytes");
    }
    if (device < -1) {
        throw InvalidArgument("CUDA device must be -1 or a non-negative device index");
    }
    checkDriver(cuInit(0), "cuInit");
    if (device == -1) {
        CUDA_CHECK(cudaGetDevice(&device));
    }

    auto state = std::make_shared<detail::CudaPtxModuleState>();
    checkDriver(cuDeviceGet(&state->device, device), "cuDeviceGet");
    checkDriver(cuDevicePrimaryCtxRetain(&state->context, state->device), "cuDevicePrimaryCtxRetain");
    ContextScope context(state->context);

    // PTX returned by a compiler blob need not have a terminating null byte.
    const std::string source(ptx);
    std::array<char, 8192> errorLog{};
    CUjit_option options[] = { CU_JIT_ERROR_LOG_BUFFER, CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES };
    void *values[] = { errorLog.data(), reinterpret_cast<void *>(static_cast<std::uintptr_t>(errorLog.size())) };
    const CUresult result = cuModuleLoadDataEx(&state->module, source.c_str(), 2, options, values);
    errorLog.back() = '\0';
    checkDriver(result, "cuModuleLoadDataEx", errorLog.data());
    return CudaPtxModule(std::move(state));
}

CudaPtxKernel CudaPtxModule::getKernel(std::string_view name) const {
    if (!state_) {
        throw InvalidArgument("Cannot get a kernel from a moved-from PTX module");
    }
    if (name.empty() || name.find('\0') != std::string_view::npos) {
        throw InvalidArgument("PTX entry-point name must be non-empty and contain no null bytes");
    }
    ContextScope context(state_->context);
    CUfunction function = nullptr;
    const std::string entryPoint(name);
    checkDriver(cuModuleGetFunction(&function, state_->module, entryPoint.c_str()),
                "cuModuleGetFunction(" + entryPoint + ")");
    return CudaPtxKernel(state_, function);
}

void CudaPtxModule::setGlobalData(std::string_view name, std::span<const std::byte> data) const {
    if (!state_) {
        throw InvalidArgument("Cannot set globals on a moved-from PTX module");
    }
    if (name.empty() || name.find('\0') != std::string_view::npos) {
        throw InvalidArgument("PTX global symbol name must be non-empty and contain no null bytes");
    }
    ContextScope context(state_->context);
    const std::string symbol(name);
    CUdeviceptr address = 0;
    std::size_t size = 0;
    checkDriver(cuModuleGetGlobal(&address, &size, state_->module, symbol.c_str()),
                "cuModuleGetGlobal(" + symbol + ")");
    if (data.size() != size) {
        throw InvalidArgument("PTX global " + symbol + " requires " + std::to_string(size) + " bytes, received "
                              + std::to_string(data.size()));
    }
    if (!data.empty()) {
        checkDriver(cuMemcpyHtoD(address, data.data(), data.size()), "cuMemcpyHtoD(" + symbol + ")");
    }
}

CudaPtxKernel::CudaPtxKernel(std::shared_ptr<detail::CudaPtxModuleState> state, void *function)
    : state_(std::move(state))
    , function_(function) {
}

void CudaPtxKernel::launch(dim3 grid, dim3 block, std::span<void *> arguments, cudaStream_t stream,
                           unsigned int sharedMemoryBytes) const {
    if (!state_ || !function_) {
        throw InvalidArgument("Cannot launch a moved-from PTX kernel");
    }
    if (!grid.x || !grid.y || !grid.z || !block.x || !block.y || !block.z) {
        throw InvalidArgument("CUDA grid and block dimensions must be non-zero");
    }
    for (void *argument : arguments) {
        if (!argument) {
            throw InvalidArgument("Kernel arguments must point to host values");
        }
    }
    ContextScope context(state_->context);
    if (stream) {
        CUcontext streamContext = nullptr;
        checkDriver(cuStreamGetCtx(stream, &streamContext), "cuStreamGetCtx");
        if (streamContext != state_->context) {
            throw InvalidArgument("CUDA stream must belong to the PTX module's primary context");
        }
    }
    checkDriver(cuLaunchKernel(static_cast<CUfunction>(function_), grid.x, grid.y, grid.z, block.x, block.y, block.z,
                               sharedMemoryBytes, stream, arguments.empty() ? nullptr : arguments.data(), nullptr),
                "cuLaunchKernel");
}

}  // namespace libcbct
