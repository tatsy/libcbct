#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_CUDA_UTILS_H
#define LIBCBCT_CUDA_UTILS_H

#include "Common/Logging.h"

#if defined(LIBCBCT_WITH_CUDA)
#include <cuda_runtime.h>
#include <cufft.h>
#endif  // LIBCBCT_WITH_CUDA

// -----------------------------------------------------------------------------
// CUDA and error check
// -----------------------------------------------------------------------------

#define CUDA_CHECK(call)                                                                                         \
    do {                                                                                                         \
        cudaError_t rc_ = (cudaError_t)(call);                                                                   \
        if (rc_ != cudaSuccess) {                                                                                \
            throw ::libcbct::CudaError(                                                                          \
                (int)rc_, ::libcbct::STR_FMT("CUDA error {} ({}: line {}): {}", cudaGetErrorName(rc_), __FILE__, \
                                             __LINE__, cudaGetErrorString(rc_)));                                \
        }                                                                                                        \
    } while (false)

#define CUDA_CHECK_NOEXCEPT(call) \
    do {                          \
        cuda##call;               \
    } while (false)

#define CUFFT_CHECK(call)                                                                                            \
    do {                                                                                                             \
        const cufftResult rc_ = (call);                                                                              \
        if (rc_ != CUFFT_SUCCESS) {                                                                                  \
            throw ::libcbct::CudaError((int)rc_,                                                                     \
                                       ::libcbct::STR_FMT("cuFFT error {} at {}:{}", (int)rc_, __FILE__, __LINE__)); \
        }                                                                                                            \
    } while (false)

#define CUDA_SYNC_CHECK()                    \
    do {                                     \
        CUDA_CHECK(cudaGetLastError());      \
        CUDA_CHECK(cudaDeviceSynchronize()); \
    } while (false)

namespace libcbct {

inline void showCudaInfo() {
#if defined(LIBCBCT_WITH_CUDA)
    int deviceCount = 0;
    cudaGetDeviceCount(&deviceCount);
    if (deviceCount == 0) {
        LIBCBCT_INFO("No CUDA devices found.");
        return;
    }

    cudaDeviceProp prop;
    for (int i = 0; i < deviceCount; ++i) {
        cudaGetDeviceProperties(&prop, i);
        LIBCBCT_INFO("CUDA device #{}: {}", i + 1, prop.name);
    }
#else
    LIBCBCT_WARN("LibCBCT is not compiled with CUDA enabled!!");
#endif
}

}  // namespace libcbct

#endif  // LIBCBCT_CUDA_UTILS_H
