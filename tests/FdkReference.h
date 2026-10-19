#pragma once

#include <algorithm>
#include <cmath>
#include <span>
#include <stdexcept>
#include <vector>

#include "Common/Constants.h"
#include "Reconstruction/ReconstructionBase.h"

namespace libcbct::testing_utils::fdk_reference {

// Small, deliberately slow mathematical oracle. Do not replace these formulas
// with production FFT, weighting, geometry, or interpolation helpers.
inline double cosineWeight(const Geometry &geometry, int x, int y) {
    const double u = (x + 0.5 - geometry.detSize.x * 0.5) * geometry.pixSize.x;
    const double v = (y + 0.5 - geometry.detSize.y * 0.5) * geometry.pixSize.y;
    const double sdd = geometry.sdd;
    return sdd / std::sqrt(sdd * sdd + u * u + v * v);
}

// Spatial kernel INCLUDING the detector pitch factor, but no FFT normalization.
inline double rampKernel(int offset, double pitch, RampFilter filter) {
    const double pi = static_cast<double>(kPi);
    const double n = offset;
    switch (filter) {
    case RampFilter::RamLak:
        if (offset == 0) return 0.25 / pitch;
        return offset % 2 ? -1.0 / (pi * pi * n * n * pitch) : 0.0;
    case RampFilter::SheppLogan:
        return -2.0 / (pi * pi * pitch * (4.0 * n * n - 1.0));
    }
    throw std::invalid_argument("Unknown reference ramp filter");
}

// Zero extension followed by linear convolution and a crop to detector width.
// Each row is independent; no padded array or FFT is used by this oracle.
inline std::vector<double> filteredProjection(const VolumeF32 &sinogram, const Geometry &geometry,
                                              int projection, RampFilter filter) {
    const int width = geometry.detSize.x, height = geometry.detSize.y;
    const double pitch = static_cast<double>(geometry.pixSize.x) * geometry.sod / geometry.sdd;
    std::vector<double> output(width * height, 0.0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int j = 0; j < width; ++j) {
                output[y * width + x] += sinogram(j, y, projection) * cosineWeight(geometry, j, y)
                                         * rampKernel(x - j, pitch, filter);
            }
        }
    }
    return output;
}

// FDK-specific distance weighting, NOT an adjoint projector. Coordinates are
// detector-edge coordinates: [0,width) x [0,height), with centers at i+0.5.
// The first/last bilinear cell extends half a pixel beyond its sample centers.
inline void backprojectAdd(std::span<const double> filtered, const Geometry &geometry,
                           double theta, double scale, VolumeF64 &volume) {
    const int width = geometry.detSize.x, height = geometry.detSize.y;
    const auto size = geometry.volSize;
    const double spacing = static_cast<double>(width) * geometry.pixSize.x * geometry.sod
                           / geometry.sdd / size.x;
    const double c = std::cos(theta), s = std::sin(theta);
    for (int z = 0; z < size.z; ++z) {
        for (int y = 0; y < size.y; ++y) {
            for (int x = 0; x < size.x; ++x) {
                const double vx = (x + 0.5 - size.x * 0.5) * spacing;
                const double vy = (y + 0.5 - size.y * 0.5) * spacing;
                const double vz = (z + 0.5 - size.z * 0.5) * spacing;
                const double depth = vx * c - vy * s + geometry.sod;
                const double u = (vx * s + vy * c) * geometry.sdd / depth / geometry.pixSize.x + width * 0.5;
                const double v = vz * geometry.sdd / depth / geometry.pixSize.y + height * 0.5;
                if (u < 0 || u >= width || v < 0 || v >= height) continue;

                const double px = u - 0.5, py = v - 0.5;
                const int x0 = std::clamp(static_cast<int>(std::floor(px)), 0, width - 2);
                const int y0 = std::clamp(static_cast<int>(std::floor(py)), 0, height - 2);
                const double fx = px - x0, fy = py - y0;
                const double sample = (1 - fx) * (1 - fy) * filtered[y0 * width + x0]
                                      + fx * (1 - fy) * filtered[y0 * width + x0 + 1]
                                      + (1 - fx) * fy * filtered[(y0 + 1) * width + x0]
                                      + fx * fy * filtered[(y0 + 1) * width + x0 + 1];
                const double distanceWeight = geometry.sod / depth;
                volume(x, y, z) += sample * distanceWeight * distanceWeight * scale;
            }
        }
    }
}

inline VolumeF64 reconstruct(const VolumeF32 &sinogram, const Geometry &geometry, RampFilter filter) {
    VolumeF64 volume(geometry.volSize.x, geometry.volSize.y, geometry.volSize.z);
    const int count = static_cast<int>(sinogram.size<2>());
    for (int p = 0; p < count; ++p) {
        const auto filtered = filteredProjection(sinogram, geometry, p, filter);
        backprojectAdd(filtered, geometry, static_cast<double>(kTwoPi) * p / count,
                       static_cast<double>(kPi) / count, volume);
    }
    return volume;
}

}  // namespace libcbct::testing_utils::fdk_reference
