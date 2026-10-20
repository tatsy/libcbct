#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_CUDA_BUFFER_STORAGE_H
#define LIBCBCT_CUDA_BUFFER_STORAGE_H

#include <cuda.h>

#include <cstddef>
#include <memory>

#include "BufferStorage.h"

namespace libcbct {

class CudaDeviceState;
class CudaBufferAccess;

namespace detail {

class CudaBufferStorage : public BufferStorage {
public:
    CudaBufferStorage(ExecutionContext context, std::shared_ptr<CudaDeviceState> device, std::size_t sizeBytes);
    ~CudaBufferStorage() override;

private:
    std::shared_ptr<CudaDeviceState> device_ = nullptr;
    CUdeviceptr address_ = 0;

    friend class CudaBufferAccess;
};

}  // namespace detail

}  // namespace libcbct

#endif  // LIBCBCT_CUDA_BUFFER_STORAGE_H
