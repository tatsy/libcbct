#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_IMAGE_UTILS_H
#define LIBCBCT_IMAGE_UTILS_H

#include <math.h>

#include "CudaUtils.h"

#ifdef __CUDACC__
#define __HOST_DEVICE__ __host__ __device__ __forceinline__
#else
#ifdef _MSC_VER
#define __HOST_DEVICE__ __forceinline
#else
#define __HOST_DEVICE__ __attribute__((always_inline)) inline
#endif
#endif

__HOST_DEVICE__ int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

__HOST_DEVICE__ float bilerp(float *const image, int width, int height, float x, float y) {
    const float xf = floorf(x);
    const float yf = floorf(y);
    const int x0 = clampi((int)xf, 0, width - 2);
    const int y0 = clampi((int)yf, 0, height - 2);
    const float u = x - x0;
    const float v = y - y0;

    const int base = y0 * width + x0;

    const float v00 = image[base];
    const float v01 = image[base + 1];
    const float v10 = image[base + width];
    const float v11 = image[base + width + 1];

    const float v0 = fmaf(u, (v01 - v00), v00);
    const float v1 = fmaf(u, (v11 - v10), v10);

    return fmaf(v, (v1 - v0), v0);
}

#endif  // LIBCBCT_IMAGE_UTILS_H
