#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_VOLUME_H
#define LIBCBCT_VOLUME_H

#include <cstring>

#include "Common/Logging.h"

template <typename T>
class Volume {
public:
    explicit Volume(int sizeX = 0, int sizeY = 0, int sizeZ = 0)
        : sizeX(sizeX)
        , sizeY(sizeY)
        , sizeZ(sizeZ) {
        this->alloc();
    }

    Volume(const Volume<T> &other)
        : Volume(other.sizeX, other.sizeY, other.sizeZ) {
        std::memcpy(data, other.data, sizeof(T) * sizeX * sizeY * sizeZ);
    }

    Volume(Volume<T> &&other)
        : sizeX(other.sizeX)
        , sizeY(other.sizeY)
        , sizeZ(other.sizeZ)
        , data(other.data) {
        other.sizeX = 0;
        other.sizeY = 0;
        other.sizeZ = 0;
        other.data = NULL;
    }

    virtual ~Volume() {
        delete[] data;
    }

    Volume &operator=(const Volume<T> &other) {
        sizeX = other.sizeX;
        sizeY = other.sizeY;
        sizeZ = other.sizeZ;
        this->alloc();
        std::memcpy(data, other.data, sizeof(T) * sizeX * sizeY * sizeZ);
        return *this;
    }

    Volume &operator=(Volume<T> &&other) {
        sizeX = other.sizeX;
        sizeY = other.sizeY;
        sizeZ = other.sizeZ;
        data = other.data;

        other.sizeX = 0;
        other.sizeY = 0;
        other.sizeZ = 0;
        other.data = NULL;

        return *this;
    }

    T &operator()(int x, int y, int z) {
        LIBCBCT_ASSERT(x >= 0 && x < sizeX && y >= 0 && y < sizeY && z >= 0 && z < sizeZ,
                       "Volume index out of bounds!");
        return data[((uint64_t)z * sizeY + (uint64_t)y) * sizeX + (uint64_t)x];
    }

    T operator()(int x, int y, int z) const {
        LIBCBCT_ASSERT(x >= 0 && x < sizeX && y >= 0 && y < sizeY && z >= 0 && z < sizeZ,
                       "Volume index out of bounds!");
        return data[((uint64_t)z * sizeY + (uint64_t)y) * sizeX + (uint64_t)x];
    }

    T *const ptr() const {
        return data;
    }

    template <int Dim>
    typename std::enable_if<Dim >= 0 && Dim <= 2, uint64_t>::type
    size() const {
        return sizes_[Dim];
    }

private:
    void alloc() {
        if (data != NULL) {
            delete[] data;
        }

        if (sizeX * sizeY * sizeZ != 0) {
            data = new T[sizeX * sizeY * sizeZ];
            std::memset(data, 0, sizeof(T) * sizeX * sizeY * sizeZ);
        }
    }

    union {
        struct {
            uint64_t sizeX, sizeY, sizeZ;
        };
        uint64_t sizes_[3];
    };
    T *data = NULL;
};

using UInt16Volume = Volume<uint16_t>;
using FloatVolume = Volume<float>;

#endif  // LIBCBCT_VOLUME_H
