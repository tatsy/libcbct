#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_CUDA_UTILS_H
#define LIBCBCT_CUDA_UTILS_H

#include <cuda_runtime.h>

// -----------------------------------------------------------------------------
// CUDA and error check
// -----------------------------------------------------------------------------

#define CUDA_CHECK(call)                                                                             \
    {                                                                                                \
        cudaError_t rc = (cudaError_t)call;                                                          \
        if (rc != cudaSuccess) {                                                                     \
            std::stringstream txt;                                                                   \
            cudaError_t err = rc; /*cudaGetLastError();*/                                            \
            txt << "CUDA Error " << cudaGetErrorName(err) << " (" << cudaGetErrorString(err) << ")"; \
            std::cerr << txt.str() << std::endl;                                                     \
            exit(2);                                                                                 \
        }                                                                                            \
    }

#define CUDA_CHECK_NOEXCEPT(call) \
    {                             \
        cuda##call;               \
    }

#define CUDA_SYNC_CHECK()                                                                                \
    {                                                                                                    \
        cudaDeviceSynchronize();                                                                         \
        cudaError_t error = cudaGetLastError();                                                          \
        if (error != cudaSuccess) {                                                                      \
            fprintf(stderr, "error (%s: line %d): %s\n", __FILE__, __LINE__, cudaGetErrorString(error)); \
            exit(2);                                                                                     \
        }                                                                                                \
    }

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
        LIBCBCT_INFO("CUDA device #%d: %s", i + 1, prop.name);
    }
#else
    LIBCBCT_WARN("LibCBCT is not compiled with CUDA enabled!!");
#endif
}

#endif  // LIBCBCT_CUDA_UTILS_H
