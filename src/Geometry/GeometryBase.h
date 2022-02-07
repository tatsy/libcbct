#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_GEOMETRY_BASE_H
#define LIBCBCT_GEOMETRY_BASE_H

#include "Common/Api.h"
#include "Utils/Vec.h"
#include "Utils/CudaUtils.h"

struct LIBCBCT_API Geometry {
    Geometry() {
    }

    Geometry(const vec2i &detSize, const vec2f &pixSize, const vec3i &volSize, float sod, float sdd)
        : detSize(detSize)
        , pixSize(pixSize)
        , volSize(volSize)
        , sod(sod)
        , sdd(sdd) {
    }

    vec2i detSize;
    vec2f pixSize;
    vec3i volSize;
    float sod;
    float sdd;
};

__both__ inline vec3f project(const vec3f &xyz, float theta, const Geometry& geom) {
    const float cosTheta = cosf(theta);
    const float sinTheta = sinf(theta);
    const float vx = xyz.x * cosTheta - xyz.y * sinTheta + geom.sod;
    const float vy = xyz.x * sinTheta + xyz.y * cosTheta;
    const float vz = xyz.z;
    const float u = vy * geom.sdd / fabsf(vx);
    const float v = vz * geom.sdd / fabsf(vx);
    const float w = geom.sdd / sqrtf(geom.sdd * geom.sdd + u * u + v * v);

    const vec2f detUV = vec2f(u, v) / geom.pixSize + vec2f(geom.detSize) * 0.5f;
    return vec3f(detUV.x, detUV.y, w);
}

__both__ inline vec3f vox2pix(const vec3i &xyz, float theta, const Geometry &geom) {
    const float cx = geom.volSize.x * 0.5f;
    const float cy = geom.volSize.y * 0.5f;
    const float cz = geom.volSize.z * 0.5f;
    const float voxelSize = (geom.detSize.x * geom.pixSize.x) * (geom.sod / geom.sdd) / geom.volSize.x;

    const vec3f v = (vec3f(xyz) - vec3f(cx, cy, cz)) * voxelSize;
    return project(v, theta, geom);
}


#endif  // LIBCBCT_GEOMETRY_BASE_H
