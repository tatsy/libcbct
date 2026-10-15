#include <gtest/gtest.h>

#include "Common/Constants.h"
#include "Geometry/GeometryBase.h"

namespace libcbct::tests {

namespace {
const Geometry kGeom(vec2i(128, 96), vec2f(0.8f, 0.8f), vec3i(64, 64, 64), 100.0f, 400.0f);
}

// The rotation center always projects onto the detector center with unit distance weight
TEST(Geometry, IsocenterProjectsToDetectorCenter) {
    for (float theta : { 0.0f, 0.7f, 2.0f, 4.5f }) {
        const vec3f uvw = project(vec3f(0.0f, 0.0f, 0.0f), theta, kGeom);
        EXPECT_NEAR(uvw.x, 64.0f, 1e-4f) << "theta=" << theta;
        EXPECT_NEAR(uvw.y, 48.0f, 1e-4f) << "theta=" << theta;
        EXPECT_NEAR(uvw.z, 1.0f, 1e-6f) << "theta=" << theta;
    }
}

// A point on the rotation axis is magnified by SDD / SOD regardless of the angle
TEST(Geometry, PointOnAxisIsMagnified) {
    const float z = 5.0f;
    for (float theta : { 0.0f, 1.0f, 3.0f }) {
        const vec3f uvw = project(vec3f(0.0f, 0.0f, z), theta, kGeom);
        EXPECT_NEAR((uvw.y - 48.0f) * 0.8f, z * 400.0f / 100.0f, 1e-3f) << "theta=" << theta;
    }
}

// Distance weight (SOD / L)^2: > 1 near the source, < 1 near the detector
TEST(Geometry, DistanceWeightDependsOnDepth) {
    const vec3f nearSource = project(vec3f(-10.0f, 0.0f, 0.0f), 0.0f, kGeom);
    const vec3f nearDetector = project(vec3f(10.0f, 0.0f, 0.0f), 0.0f, kGeom);
    EXPECT_NEAR(nearSource.z, (100.0f / 90.0f) * (100.0f / 90.0f), 1e-5f);
    EXPECT_NEAR(nearDetector.z, (100.0f / 110.0f) * (100.0f / 110.0f), 1e-5f);
}

// Voxel centers are symmetric around the volume center
TEST(Geometry, VoxelCentersAreSymmetric) {
    const int n = kGeom.volSize.x;
    const float theta = 0.5f * (float)libcbct::kPi;
    for (int i : { 0, 10, 31 }) {
        const vec3f a = vox2pix(vec3i(i, n / 2, n / 2), theta, kGeom);
        const vec3f b = vox2pix(vec3i(n - 1 - i, n / 2, n / 2), theta, kGeom);
        EXPECT_NEAR(a.x - 64.0f, -(b.x - 64.0f), 1e-3f) << "i=" << i;
    }
}

}  // namespace libcbct::tests
