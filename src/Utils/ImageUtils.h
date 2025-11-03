#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_IMAGE_UTILS_H
#define LIBCBCT_IMAGE_UTILS_H

#include <math.h>

#include "CudaUtils.h"

__both__ inline float bilerp(float *const image, int width, int height, float x, float y) {
#ifndef __CUDACC__
    using std::max;
    using std::min;
#endif
    const int x0 = max(0, (int)x);
    const int y0 = max(0, (int)y);
    const int x1 = min((int)x + 1, width - 1);
    const int y1 = min((int)y + 1, height - 1);
    const float u = x - x0;
    const float v = y - y0;

    const float v00 = image[y0 * width + x0];
    const float v01 = image[y0 * width + x1];
    const float v10 = image[y1 * width + x0];
    const float v11 = image[y1 * width + x1];

    const float v0 = (1.0f - u) * v00 + u * v01;
    const float v1 = (1.0f - u) * v10 + u * v11;

    return (1.0f - v) * v0 + v * v1;
}

#endif  // LIBCBCT_IMAGE_UTILS_H
