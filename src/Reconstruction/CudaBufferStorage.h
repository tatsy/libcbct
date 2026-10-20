#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_CUDA_BUFFER_STORAGE_H
#define LIBCBCT_CUDA_BUFFER_STORAGE_H

#include <cstddef>
#include <memory>

#include "BufferStorage.h"

namespace libcbct::detail {

class CudaDeviceState;
class CudaBufferAccess;

class CudaBufferStorage : public BufferStorage {
public:
    CudaBufferStorage(ExecutionContext context, std::shared_ptr<CudaDeviceState> device, std::size_t sizeBytes);
    ~CudaBufferStorage() override;

private:
    std::shared_ptr<CudaDeviceState> device_;
    CUdeviceptr address_ = 0;

    friend class CudaBufferAccess;
};

}  // namespace libcbct::detail

#endif  // LIBCBCT_CUDA_BUFFER_STORAGE_H
