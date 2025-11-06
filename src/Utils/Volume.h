#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_VOLUME_H
#define LIBCBCT_VOLUME_H

#include <cstring>
#include <memory>
#include <algorithm>
#include <functional>

#include "Common/Logging.h"
#include "Common/OpenMP.h"
#include "Utils/ImageUtils.h"

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
        std::memcpy(data.get(), other.data.get(), sizeof(T) * sizeX * sizeY * sizeZ);
    }

    Volume(Volume<T> &&other)
        : sizeX(other.sizeX)
        , sizeY(other.sizeY)
        , sizeZ(other.sizeZ)
        , data(std::move(other.data)) {
        other.sizeX = 0;
        other.sizeY = 0;
        other.sizeZ = 0;
        other.data = nullptr;
    }

    virtual ~Volume() = default;

    Volume &operator=(const Volume<T> &other) {
        sizeX = other.sizeX;
        sizeY = other.sizeY;
        sizeZ = other.sizeZ;
        this->resize(sizeX, sizeY, sizeZ);
        std::memcpy(data.get(), other.data.get(), sizeof(T) * sizeX * sizeY * sizeZ);
        return *this;
    }

    Volume &operator=(Volume<T> &&other) {
        sizeX = other.sizeX;
        sizeY = other.sizeY;
        sizeZ = other.sizeZ;
        data = std::move(other.data);

        other.sizeX = 0;
        other.sizeY = 0;
        other.sizeZ = 0;
        other.data = nullptr;

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

        const float xf = floorf(x);
        const float yf = floorf(y);
        const float zf = floorf(z);

        const int ix = clampi((int)xf, 0, (int)sizeX - 2);
        const int iy = clampi((int)yf, 0, (int)sizeY - 2);
        const int iz = clampi((int)zf, 0, (int)sizeZ - 2);
        const float u = x - ix;
        const float v = y - iy;
        const float w = z - iz;

        const uint64_t base = (uint64_t)iz * sizeY * sizeX + (uint64_t)iy * sizeX + (uint64_t)ix;
        const float v000 = data[base];
        const float v001 = data[base + 1];
        const float v010 = data[base + sizeX];
        const float v011 = data[base + sizeX + 1];
        const float v100 = data[base + sizeX * sizeY];
        const float v101 = data[base + sizeX * sizeY + 1];
        const float v110 = data[base + sizeX * sizeY + sizeX];
        const float v111 = data[base + sizeX * sizeY + sizeX + 1];

        const float v00 = fmaf(u, (v001 - v000), v000);
        const float v10 = fmaf(u, (v011 - v010), v010);
        const float v01 = fmaf(u, (v101 - v100), v100);
        const float v11 = fmaf(u, (v111 - v110), v110);

        const float v0 = fmaf(v, (v10 - v00), v00);
        const float v1 = fmaf(v, (v11 - v01), v01);

        return fmaf(w, (v1 - v0), v0);
    }

    T *const ptr() const {
        return data.get();
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
        auto buf = std::make_unique<T[]>(sizeZ);
        std::fill_n(buf.get(), sizeZ, init);

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
    typename std::enable_if<Dim >= 0 && Dim <= 2, uint64_t>::type size() const {
        return sizes_[Dim];
    }

    void resize(uint64_t sizeX, uint64_t sizeY, uint64_t sizeZ) {
        this->sizeX = sizeX;
        this->sizeY = sizeY;
        this->sizeZ = sizeZ;

        if (!data) {
            data.reset(nullptr);
        }

        if (sizeX * sizeY * sizeZ != 0) {
            data = std::make_unique<T[]>(sizeX * sizeY * sizeZ);
            std::memset(data.get(), 0, sizeof(T) * sizeX * sizeY * sizeZ);
        }
    }

private:
    union {
        struct {
            uint64_t sizeX;
            uint64_t sizeY;
            uint64_t sizeZ;
        };
        uint64_t sizes_[3];
    };
    std::unique_ptr<T[]> data = nullptr;
};

using VolumeU8 = Volume<uint8_t>;
using VolumeU16 = Volume<uint16_t>;
using VolumeU32 = Volume<uint32_t>;
using VolumeF32 = Volume<float>;
using VolumeF64 = Volume<double>;

#endif  // LIBCBCT_VOLUME_H
