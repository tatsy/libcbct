#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_VOLUME_H
#define LIBCBCT_VOLUME_H

#include <cstring>
#include <functional>

#include "Common/Logging.h"
#include "Common/OpenMP.h"

template <typename T>
class Volume {
public:
    explicit Volume(int sizeX = 0, int sizeY = 0, int sizeZ = 0)
        : sizeX(sizeX)
        , sizeY(sizeY)
        , sizeZ(sizeZ) {
        resize(sizeX, sizeY, sizeZ);
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

    T operator()(float x, float y, float z) const {
        LIBCBCT_ASSERT(x >= 0 && x < sizeX && y >= 0 && y < sizeY && z >= 0 && z < sizeZ,
                       "Volume index out of bounds!");

        x = std::max(0.0f, std::min(x - 0.5f, (float)sizeX));
        y = std::max(0.0f, std::min(y - 0.5f, (float)sizeY));
        z = std::max(0.0f, std::min(z - 0.5f, (float)sizeZ));
        
        const int ix = std::min((int)x, (int)sizeX - 2);
        const int iy = std::min((int)y, (int)sizeY - 2);
        const int iz = std::min((int)z, (int)sizeZ - 2);
        const float u = x - ix;
        const float v = y - iy;
        const float w = z - iz;
        
        const float v00 = (1.0f - u) * (*this)(ix, iy, iz) + u * (*this)(ix + 1, iy, iz);
        const float v01 = (1.0f - u) * (*this)(ix, iy, iz + 1) + u * (*this)(ix + 1, iy, iz + 1);
        const float v10 = (1.0f - u) * (*this)(ix, iy + 1, iz) + u * (*this)(ix + 1, iy + 1, iz);
        const float v11 = (1.0f - u) * (*this)(ix, iy + 1, iz + 1) + u * (*this)(ix + 1, iy + 1, iz + 1);

        const float v0 = (1.0f - v) * v00 + v * v10;
        const float v1 = (1.0f - v) * v01 + v * v11;

        return (1.0f - w) * v0 + w * v1;
    }

    T *const ptr() const {
        return data;
    }

    void forEach(const typename std::function<T(T)> &func) {
        OMP_PARALLEL_FOR(int z = 0; z < sizeZ; z++) {
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    (*this)(x, y, z) = func((*this)(x, y, z));
                }
            }
        }
    }

    T reduce(const typename std::function<T(T, T)> &func, const T &init) const {
        std::vector<T> buf(sizeZ, init);
        OMP_PARALLEL_FOR(int z = 0; z < sizeZ; z++) {
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    buf[z] = func(buf[z], (*this)(x, y, z));
                }
            }
        }

        T ret = init;
        for (int z = 0; z < sizeZ; z++) {
            ret = func(ret, buf[z]);
        }

        return ret;
    }

    template <int Dim>
    typename std::enable_if<Dim >= 0 && Dim <= 2, uint64_t>::type
    size() const {
        return sizes_[Dim];
    }

    void resize(uint64_t sizeX, uint64_t sizeY, uint64_t sizeZ) {
        this->sizeX = sizeX;
        this->sizeY = sizeY;
        this->sizeZ = sizeZ;

        if (data != NULL) {
            delete[] data;
        }

        if (sizeX * sizeY * sizeZ != 0) {
            data = new T[sizeX * sizeY * sizeZ];
            std::memset(data, 0, sizeof(T) * sizeX * sizeY * sizeZ);
        }
    }

private:
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
