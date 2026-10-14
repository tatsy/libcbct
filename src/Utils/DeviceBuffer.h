#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_DEVICE_BUFFER_H
#define LIBCBCT_DEVICE_BUFFER_H

#include <cuda_runtime.h>

#include <utility>

#include "CudaUtils.h"

template <typename T>
class DeviceBuffer {
public:
    explicit DeviceBuffer(size_t size)
        : size_(size) {
        CUDA_CHECK(cudaMalloc(&ptr_, size_ * sizeof(T)));
    }

    ~DeviceBuffer() {
        if (ptr_) {
            cudaFree(ptr_);
        }
    }

    DeviceBuffer(const DeviceBuffer &) = delete;
    DeviceBuffer &operator=(const DeviceBuffer &) = delete;

    DeviceBuffer(DeviceBuffer &&other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr))
        , size_(std::exchange(other.size_, 0)) {
    }

    T *ptr() const {
        return ptr_;
    }

    size_t size() const {
        return size_;
    }

private:
    T *ptr_ = nullptr;
    size_t size_ = 0;
};

#endif  // LIBCBCT_DEVICE_BUFFER_H
