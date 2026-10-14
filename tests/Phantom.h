#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Common/Constants.h"
#include "Geometry/GeometryBase.h"
#include "Utils/Volume.h"

namespace testing_utils {

struct Sphere {
    double cx, cy, cz, radius, mu;
};

// Small cone-beam setup shared by the reconstruction tests (magnification 4)
struct PhantomSetup {
    int detSize = 128;
    float pixSize = 0.8f;
    int volSize = 64;
    int nProj = 180;
    float sod = 100.0f;
    float sdd = 400.0f;

    Geometry geometry() const {
        return Geometry(vec2i(detSize, detSize), vec2f(pixSize, pixSize), vec3i(volSize, volSize, volSize), sod, sdd);
    }
    double voxelSize() const {
        return detSize * pixSize * (sod / sdd) / volSize;
    }
};

// Analytic line integrals through a set of spheres, using the same conventions as project()
inline VolumeF32 makeSphereSinogram(const PhantomSetup &s, const std::vector<Sphere> &spheres) {
    VolumeF32 sino(s.detSize, s.detSize, s.nProj);
    for (int i = 0; i < s.nProj; i++) {
        const double theta = 2.0 * (double)libcbct::kPi * i / s.nProj;
        const double c = std::cos(theta), sn = std::sin(theta);
        for (int y = 0; y < s.detSize; y++) {
            for (int x = 0; x < s.detSize; x++) {
                const double u = (x + 0.5 - 0.5 * s.detSize) * s.pixSize;
                const double v = (y + 0.5 - 0.5 * s.detSize) * s.pixSize;
                const double norm = std::sqrt((double)s.sdd * s.sdd + u * u + v * v);
                const double d[3] = { s.sdd / norm, u / norm, v / norm };
                double p = 0.0;
                for (const Sphere &sp : spheres) {
                    // Sphere center in the rotated frame, measured from the source
                    const double oc[3] = { sp.cx * c - sp.cy * sn + s.sod, sp.cx * sn + sp.cy * c, sp.cz };
                    const double t = d[0] * oc[0] + d[1] * oc[1] + d[2] * oc[2];
                    const double dist2 = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - t * t;
                    p += sp.mu * 2.0 * std::sqrt(std::max(0.0, sp.radius * sp.radius - dist2));
                }
                sino(x, y, i) = (float)p;
            }
        }
    }
    return sino;
}

// Mean value inside a sphere shrunk by `shrink`, to avoid partial-volume voxels at the boundary
inline double meanInsideSphere(const VolumeF32 &vol, const PhantomSetup &s, const Sphere &sp, double shrink = 0.6) {
    const double vs = s.voxelSize();
    const double r2 = (shrink * sp.radius) * (shrink * sp.radius);
    double sum = 0.0;
    long count = 0;
    for (int z = 0; z < s.volSize; z++) {
        for (int y = 0; y < s.volSize; y++) {
            for (int x = 0; x < s.volSize; x++) {
                const double px = (x + 0.5 - 0.5 * s.volSize) * vs - sp.cx;
                const double py = (y + 0.5 - 0.5 * s.volSize) * vs - sp.cy;
                const double pz = (z + 0.5 - 0.5 * s.volSize) * vs - sp.cz;
                if (px * px + py * py + pz * pz < r2) {
                    sum += vol(x, y, z);
                    count++;
                }
            }
        }
    }
    return sum / count;
}

inline double maxAbsDiff(const VolumeF32 &a, const VolumeF32 &b) {
    const size_t n = a.size<0>() * a.size<1>() * a.size<2>();
    double m = 0.0;
    for (size_t i = 0; i < n; i++) {
        m = std::max(m, (double)std::fabs(a.ptr()[i] - b.ptr()[i]));
    }
    return m;
}

}  // namespace testing_utils
