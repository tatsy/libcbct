#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_VOLUME_H
#define LIBCBCT_VOLUME_H

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

    ~Volume() {
        delete[] data;
    }

    T &operator(int x, int y, int z) {
        LIBCBCT_ASSERT(x >= 0 && x < sizeX && y >= 0 && y < sizeY && z >= 0 && z < sizeZ, "Volume index out of bounds!");
        return data[(z * sizeY + y) * sizeX + x];
    }

    T operator(int x, int y, int z) const {
        LIBCBCT_ASSERT(x >= 0 && x < sizeX && y >= 0 && y < sizeY && z >= 0 && z < sizeZ, "Volume index out of bounds!");
        return data[(z * sizeY + y) * sizeX + x];
    }

private:
    void alloc() {
        if (sizeX * sizeY * sizeZ != 0) {
            data = new T[sizeX * sizeY * sizeZ];
            std::memset(data, 0, sizeof(T) * sizeX * sizeY * sizeZ);
        }
    }

    int sizeX, sizeY, sizeZ;
    T *data = NULL;
};

#endif  // LIBCBCT_VOLUME_H
