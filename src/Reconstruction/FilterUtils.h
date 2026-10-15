#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_FILTER_UTILS_H
#define LIBCBCT_FILTER_UTILS_H

#include <vector>

#include "ReconstructionBase.h"

namespace libcbct {

//! Smallest power of two >= 2 * detWidth, so that circular convolution equals linear convolution
LIBCBCT_API size_t paddedFFTLength(size_t detWidth);

/**
 * Frequency response of the band-limited ramp filter (nPad / 2 + 1 real coefficients).
 * dp is the detector pitch scaled to the isocenter. The 1/nPad normalization is NOT included.
 */
LIBCBCT_API std::vector<float> makeRampResponse(size_t nPad, double dp, RampFilter type);

//! FDK cosine pre-weights SDD / sqrt(SDD^2 + u^2 + v^2) for each detector pixel
LIBCBCT_API std::vector<float> makeCosineWeights(const Geometry &geometry, size_t detWidth, size_t detHeight);

}  // namespace libcbct

#endif  // LIBCBCT_FILTER_UTILS_H
