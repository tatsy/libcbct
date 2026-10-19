#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_VOLUME_H
#define LIBCBCT_VOLUME_H

#include <cstring>
#include <memory>
#include <tuple>
#include <array>
#include <vector>
#include <utility>
#include <algorithm>
#include <span>
#include <functional>

#include "Common/Exception.h"
#include "Common/Logging.h"
#include "Common/OpenMP.h"
#include "Utils/ImageUtils.h"
#include "Utils/CheckedSize.h"

namespace libcbct {

enum class VolumeType {
    Uint8,
    Uint16,
    Uint32,
    Float32,
    Float64,
};

template <typename T>
class Volume {
public:
    explicit Volume(std::size_t sizeX = 0, std::size_t sizeY = 0, std::size_t sizeZ = 0) {
        resize(static_cast<std::size_t>(sizeX), static_cast<std::size_t>(sizeY), static_cast<std::size_t>(sizeZ));
    }

    Volume(const Volume<T> &other)
        : Volume(other.shape_[0], other.shape_[1], other.shape_[2]) {
        std::memcpy(data_.get(), other.data_.get(), sizeof(T) * other.numel());
    }

    Volume(Volume<T> &&other)
        : shape_{ std::exchange(other.shape_, {}) }
        , data_{ std::move(other.data_) } {
    }

    virtual ~Volume() = default;

    Volume &operator=(const Volume<T> &other) {
        if (this == &other) {
            return *this;
        }

        auto cpy = Volume<T>(other.shape_[0], other.shape_[1], other.shape_[2]);
        if (!other.empty()) {
            const auto count = detail::checkedProduct(other.shape_[0], other.shape_[1], other.shape_[2], sizeof(T));
            std::memcpy(cpy.data_.get(), other.data_.get(), count);
        }

        *this = std::move(cpy);
        return *this;
    }

    Volume &operator=(Volume<T> &&other) {
        if (this == &other) {
            return *this;
        }

        shape_ = std::exchange(other.shape_, {});
        data_ = std::move(other.data_);

        return *this;
    }

    T &operator()(int x, int y, int z) {
        LIBCBCT_ASSERT(x >= 0 && x < shape_[0] && y >= 0 && y < shape_[1] && z >= 0 && z < shape_[2],
                       "Volume index out of bounds!");
        return data_[((std::size_t)z * shape_[1] + (std::size_t)y) * shape_[0] + (std::size_t)x];
    }

    T operator()(int x, int y, int z) const {
        LIBCBCT_ASSERT(x >= 0 && x < shape_[0] && y >= 0 && y < shape_[1] && z >= 0 && z < shape_[2],
                       "Volume index out of bounds!");
        return data_[((std::size_t)z * shape_[1] + (std::size_t)y) * shape_[0] + (std::size_t)x];
    }

    T operator()(float x, float y, float z) const {
        LIBCBCT_ASSERT(x >= 0 && x < shape_[0] && y >= 0 && y < shape_[1] && z >= 0 && z < shape_[2],
                       "Volume index out of bounds!");

        const float xf = floorf(x);
        const float yf = floorf(y);
        const float zf = floorf(z);

        const int ix = clampi((int)xf, 0, (int)shape_[0] - 2);
        const int iy = clampi((int)yf, 0, (int)shape_[1] - 2);
        const int iz = clampi((int)zf, 0, (int)shape_[2] - 2);
        const float u = x - ix;
        const float v = y - iy;
        const float w = z - iz;

        const size_t base = (size_t)iz * shape_[1] * shape_[0] + (size_t)iy * shape_[0] + (size_t)ix;
        const float v000 = data_[base];
        const float v001 = data_[base + 1];
        const float v010 = data_[base + shape_[0]];
        const float v011 = data_[base + shape_[0] + 1];
        const float v100 = data_[base + shape_[0] * shape_[1]];
        const float v101 = data_[base + shape_[0] * shape_[1] + 1];
        const float v110 = data_[base + shape_[0] * shape_[1] + shape_[0]];
        const float v111 = data_[base + shape_[0] * shape_[1] + shape_[0] + 1];

        const float v00 = fmaf(u, (v001 - v000), v000);
        const float v10 = fmaf(u, (v011 - v010), v010);
        const float v01 = fmaf(u, (v101 - v100), v100);
        const float v11 = fmaf(u, (v111 - v110), v110);

        const float v0 = fmaf(v, (v10 - v00), v00);
        const float v1 = fmaf(v, (v11 - v01), v01);

        return fmaf(w, (v1 - v0), v0);
    }

    T *ptr() noexcept {
        return data_.get();
    }

    const T *ptr() const noexcept {
        return data_.get();
    }

    std::span<T> elements() noexcept {
        return { ptr(), numel() };
    }

    std::span<const T> elements() const noexcept {
        return { ptr(), numel() };
    }

    size_t numel() const {
        return (size_t)shape_[0] * (size_t)shape_[1] * (size_t)shape_[2];
    }

    bool empty() const {
        return numel() == 0;
    }

    template <typename Func>
        requires std::invocable<Func &, T> && std::convertible_to<std::invoke_result_t<Func &, T>, T>
    void forEach(Func &&func) {
        OMP_PARALLEL_FOR(int z = 0; z < (int)shape_[2]; z++) {
            for (int y = 0; y < (int)shape_[1]; y++) {
                for (int x = 0; x < (int)shape_[0]; x++) {
                    (*this)(x, y, z) = func((*this)(x, y, z));
                }
            }
        }
    }

    T reduce(const typename std::function<T(T, T)> &func, const T &init) const {
        auto buf = std::make_unique<T[]>(shape_[2]);

        OMP_PARALLEL_FOR(int z = 0; z < (int)shape_[2]; z++) {
            buf[z] = init;
            for (int y = 0; y < (int)shape_[1]; y++) {
                for (int x = 0; x < (int)shape_[0]; x++) {
                    buf[z] = func(buf[z], (*this)(x, y, z));
                }
            }
        }

        T ret = init;
        for (int z = 0; z < (int)shape_[2]; z++) {
            ret = func(ret, buf[z]);
        }

        return ret;
    }

    T getMin() const {
        if (this->empty()) {
            LIBCBCT_THROW<libcbct::InvalidArgument>("Volume is empty, cannot compute minimum value.");
        }
        return reduce([](T a, T b) -> T { return std::min(a, b); }, (*this)(0, 0, 0));
    }

    T getMax() const {
        if (this->empty()) {
            LIBCBCT_THROW<libcbct::InvalidArgument>("Volume is empty, cannot compute maximum value.");
        }
        return reduce([](T a, T b) -> T { return std::max(a, b); }, (*this)(0, 0, 0));
    }

    std::tuple<T, T> getMinMax() const {
        if (this->empty()) {
            LIBCBCT_THROW<libcbct::InvalidArgument>("Volume is empty, cannot compute minimum and maximum values.");
        }

        std::vector<T> localMins(shape_[2]);
        std::vector<T> localMaxs(shape_[2]);
        const T initMin = (*this)(0, 0, 0);
        const T initMax = (*this)(0, 0, 0);
        OMP_PARALLEL_FOR(int z = 0; z < (int)shape_[2]; z++) {
            T localMin = initMin;
            T localMax = initMax;

            for (int y = 0; y < (int)shape_[1]; y++) {
                for (int x = 0; x < (int)shape_[0]; x++) {
                    const T val = (*this)(x, y, z);
                    localMin = std::min(localMin, val);
                    localMax = std::max(localMax, val);
                }
            }

            localMins[z] = localMin;
            localMaxs[z] = localMax;
        }

        T minVal = initMin;
        T maxVal = initMax;
        for (int z = 0; z < (int)shape_[2]; z++) {
            minVal = std::min(minVal, localMins[z]);
            maxVal = std::max(maxVal, localMaxs[z]);
        }

        return std::make_tuple(minVal, maxVal);
    }

    template <int Dim>
        requires(Dim >= 0 && Dim < 3)
    [[nodiscard]] std::size_t size() const noexcept {
        return shape_[Dim];
    }

    void resize(std::size_t x, std::size_t y, std::size_t z) {
        const auto count = detail::checkedProduct(x, y, z);
        (void)detail::checkedProduct(count, sizeof(T));

        auto next = count ? std::make_unique<T[]>(count) : std::unique_ptr<T[]>();

        data_ = std::move(next);
        shape_ = { x, y, z };
    }

    VolumeType type() const;

private:
    std::array<size_t, 3> shape_;
    std::unique_ptr<T[]> data_ = nullptr;
};

using VolumeU8 = Volume<uint8_t>;
using VolumeU16 = Volume<uint16_t>;
using VolumeU32 = Volume<uint32_t>;
using VolumeF32 = Volume<float>;
using VolumeF64 = Volume<double>;

template <>
inline VolumeType Volume<uint8_t>::type() const {
    return VolumeType::Uint8;
}

template <>
inline VolumeType Volume<uint16_t>::type() const {
    return VolumeType::Uint16;
}

template <>
inline VolumeType Volume<uint32_t>::type() const {
    return VolumeType::Uint32;
}

template <>
inline VolumeType Volume<float>::type() const {
    return VolumeType::Float32;
}

template <>
inline VolumeType Volume<double>::type() const {
    return VolumeType::Float64;
}

}  // namespace libcbct

#endif  // LIBCBCT_VOLUME_H
