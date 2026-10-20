#pragma once

#if defined(LIBCBCT_WITH_CUDA)

#include <cuda_runtime.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>

#include "Common/Api.h"

namespace libcbct {

namespace detail {
struct CudaPtxModuleState;
}

class CudaPtxKernel;

// Module and kernel objects share ownership of the CUDA module and primary context.
// Device allocations and streams must belong to this device's primary context.
// Complete queued work before releasing the last module/kernel or resetting the device.
class LIBCBCT_API CudaPtxModule {
public:
    // A device of -1 selects the CUDA Runtime's current device.
    static CudaPtxModule fromFile(const std::filesystem::path &path, int device = -1);
    static CudaPtxModule fromPtx(std::string_view ptx, int device = -1);

    // Use the exact entry-point name exported by the PTX file.
    CudaPtxKernel getKernel(std::string_view name) const;

    // Copy a complete global/constant symbol from host memory, using its exact layout.
    // Current Slang CUDA output uses the symbol "SLANG_globalParams" for global bindings.
    // Globals are shared by all kernels and copies of this module. Complete earlier
    // launches before changing their values; this copy does not synchronize other streams.
    void setGlobalData(std::string_view name, std::span<const std::byte> data) const;

private:
    explicit CudaPtxModule(std::shared_ptr<detail::CudaPtxModuleState> state);
    std::shared_ptr<detail::CudaPtxModuleState> state_;
};

class LIBCBCT_API CudaPtxKernel {
public:
    // Each argument points to a host value, including host variables holding device pointers.
    // The argument count, sizes, and order must match the generated PTX signature.
    // Launches are asynchronous; use cudaStreamSynchronize to observe execution errors.
    void launch(dim3 grid, dim3 block, std::span<void *> arguments = {}, cudaStream_t stream = nullptr,
                unsigned int sharedMemoryBytes = 0) const;

private:
    friend class CudaPtxModule;
    CudaPtxKernel(std::shared_ptr<detail::CudaPtxModuleState> state, void *function);
    std::shared_ptr<detail::CudaPtxModuleState> state_;
    void *function_ = nullptr;
};

}  // namespace libcbct

#endif  // LIBCBCT_WITH_CUDA
