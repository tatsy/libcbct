#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_GEOMETRY_BASE_H
#define LIBCBCT_GEOMETRY_BASE_H

#include "Common/Api.h"
#include "Utils/Vec.h"

class LIBCBCT_API GeometryBase {
public:
    GeometryBase() = default;
    GeometryBase(const vec2i &detSize, const vec2f &pixSize, const vec3i &volSize) 
        : detSize(detSize)
        , pixSize(pixSize)
        , volSize(volSize) {
    }

    virtual ~GeometryBase() = default;

    //! Project 3D world coordinates to on-screen position
    virtual vec2f project(const vec3f &xyz, float theta) const = 0;
    //! Convert voxel's integer coordinates to pixel's integer coordinates
    virtual vec2f vox2pix(const vec3i &xyz, float theta) const = 0;

    inline vec2i getDetectorSize() const {
        return detSize;
    }

    inline vec2f getPixelSize() const {
        return pixSize;
    }

    inline vec3i getVolumeSize() const {
        return volSize;
    }

protected:
    vec2i detSize;
    vec2f pixSize;
    vec3i volSize;
};

#endif  // LIBCBCT_GEOMETRY_BASE_H
