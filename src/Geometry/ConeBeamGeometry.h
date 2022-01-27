#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_CONE_BEAM_GEOMETRY_H
#define LIBCBCT_CONE_BEAM_GEOMETRY_H

#include "GeometryBase.h"

class LIBCBCT_API ConeBeamGeometry : public GeometryBase {
public:
    explicit ConeBeamGeometry(const vec2i &detSize, const vec2f &pixSize, const vec3i &volSize, float sod, float sdd)
        : GeometryBase(detSize, pixSize, volSize)
        , sod(sod)
        , sdd(sdd) {
    }

    ~ConeBeamGeometry() {
    }

    vec2f project(const vec3f &v, float theta) const override;
    vec2f vox2pix(const vec3i &v, float theta) const override;

private:
    float sod, sdd, detWidth, detHeight;
};

#endif  // LIBCBCT_CONE_BEAM_GEOMETRY_H
