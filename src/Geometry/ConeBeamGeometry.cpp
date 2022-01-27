#define LIBCBCT_API_EXPORT
#include "ConeBeamGeometry.h"

#define _USE_MATH_DEFINES
#include <math.h>

vec2f ConeBeamGeometry::project(const vec3f &xyz, float theta) const {
    const float cosTheta = cosf(theta);
    const float sinTheta = sinf(theta);
    const float vx = xyz.x * cosTheta - xyz.y * sinTheta + sod;
    const float vy = xyz.x * sinTheta + xyz.y * cosTheta;
    const float vz = xyz.z;
    const float u = vy * sdd / fabsf(vx);
    const float v = vz * sdd / fabsf(vx);
    return vec2f(u, v) / pixSize + vec2f(detSize) * 0.5f;
}

vec2f ConeBeamGeometry::vox2pix(const vec3i &xyz, float theta) const {
    const float cx = volSize.x * 0.5f;
    const float cy = volSize.y * 0.5f;
    const float cz = volSize.z * 0.5f;
    const float voxelSize = (detSize.x * pixSize.x) * (sod / sdd) / volSize.x;

    const vec3f v = (vec3f(xyz) - vec3f(cx, cy, cz)) * voxelSize;
    return project(v, theta);
}
