#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_VEC_H
#define LIBCBCT_VEC_H

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <array>
#include <algorithm>
#include <concepts>
#include <charconv>
#include <type_traits>

#ifdef __CUDACC__
#define __both__ __host__ __device__
#else
#define __both__
#endif

namespace libcbct {

// Default vector type template
template <typename T, int N>
    requires(std::is_arithmetic_v<T> && N > 0)
struct vec_t {
    std::array<T, N> t;
};

// Binary Result type: scalar type with itself always returns same type
template <typename ScalarTypeA, typename ScalarTypeB>
struct BinaryOpResultType {
    using type = std::common_type_t<ScalarTypeA, ScalarTypeB>;
};

template <typename ScalarType>
struct BinaryOpResultType<ScalarType, ScalarType> {
    using type = ScalarType;
};

template <>
struct BinaryOpResultType<int, float> {
    using type = float;
};
template <>
struct BinaryOpResultType<float, int> {
    using type = float;
};
template <>
struct BinaryOpResultType<unsigned int, float> {
    using type = float;
};
template <>
struct BinaryOpResultType<float, unsigned int> {
    using type = float;
};

template <>
struct BinaryOpResultType<int, double> {
    using type = double;
};
template <>
struct BinaryOpResultType<double, int> {
    using type = double;
};
template <>
struct BinaryOpResultType<unsigned int, double> {
    using type = double;
};
template <>
struct BinaryOpResultType<double, unsigned int> {
    using type = double;
};

template <std::integral T>
inline __both__ T divRoundUp(T a, T b) {
    return a / b + (((a % b) != 0) ? ((a ^ b) > 0) : 0);
}

// -----------------------------------------------------------------------------
// vec2
// -----------------------------------------------------------------------------

template <typename T>
struct vec_t<T, 2> {
    enum { dims = 2 };
    typedef T scalar_t;

    inline __both__ constexpr vec_t()
        : x{}
        , y{} {
    }

    inline __both__ constexpr vec_t(const T &t)
        : x(t)
        , y(t) {
    }
    inline __both__ constexpr vec_t(const T &x, const T &y)
        : x(x)
        , y(y) {
    }

#ifdef __CUDACC__
    inline __both__ constexpr vec_t(const float2 &v)
        : x(v.x)
        , y(v.y) {
    }
    inline __both__ constexpr vec_t(const int2 &v)
        : x(v.x)
        , y(v.y) {
    }
    inline __both__ constexpr vec_t(const uint2 &v)
        : x(v.x)
        , y(v.y) {
    }
    inline __both__ operator float2() const {
        return make_float2(x, y);
    }
    inline __both__ operator int2() const {
        return make_int2(x, y);
    }
    inline __both__ operator uint2() const {
        return make_uint2(x, y);
    }
#endif

    inline __both__ vec_t<T, 2> &operator=(const vec_t<T, 2> &other) = default;

    template <typename Other>
    inline __both__ explicit constexpr vec_t(const vec_t<Other, 2> &other)
        : x((T)other.x)
        , y((T)other.y) {
    }

    inline __both__ constexpr T &operator[](size_t dim) {
        switch (dim) {
        case 0:
            return x;
        default:
            return y;
        }
    }

    inline __both__ constexpr const T &operator[](size_t dim) const {
        switch (dim) {
        case 0:
            return x;
        default:
            return y;
        }
    }

    T x, y;
};

// -----------------------------------------------------------------------------
// vec3
// -----------------------------------------------------------------------------

template <typename T>
struct vec_t<T, 3> {
    enum { dims = 3 };
    typedef T scalar_t;

    inline __both__ constexpr vec_t()
        : x{}
        , y{}
        , z{} {
    }

    inline __both__ constexpr vec_t(const T &t)
        : x(t)
        , y(t)
        , z(t) {
    }
    inline __both__ constexpr vec_t(const T &x, const T &y, const T &z)
        : x(x)
        , y(y)
        , z(z) {
    }

#ifdef __CUDACC__
    inline __both__ constexpr vec_t(const float3 &v)
        : x(v.x)
        , y(v.y)
        , z(v.z) {
    }
    inline __both__ constexpr vec_t(const int3 &v)
        : x(v.x)
        , y(v.y)
        , z(v.z) {
    }
    inline __both__ constexpr vec_t(const uint3 &v)
        : x(v.x)
        , y(v.y)
        , z(v.z) {
    }
    inline __both__ operator float3() const {
        return make_float3(x, y, z);
    }
    inline __both__ operator int3() const {
        return make_int3(x, y, z);
    }
    inline __both__ operator uint3() const {
        return make_uint3(x, y, z);
    }
#endif

    inline __both__ vec_t &operator=(const vec_t &other) = default;

    template <typename Other>
    inline __both__ explicit constexpr vec_t(const vec_t<Other, 3> &other)
        : x((T)other.x)
        , y((T)other.y)
        , z((T)other.z) {
    }

    inline __both__ constexpr T &operator[](size_t dim) {
        switch (dim) {
        case 0:
            return x;
        case 1:
            return y;
        default:
            return z;
        }
    }

    inline __both__ constexpr const T &operator[](size_t dim) const {
        switch (dim) {
        case 0:
            return x;
        case 1:
            return y;
        default:
            return z;
        }
    }

    T x, y, z;
};

// -----------------------------------------------------------------------------
// vec4
// -----------------------------------------------------------------------------

template <typename T>
struct vec_t<T, 4> {
    enum { dims = 4 };
    typedef T scalar_t;

    inline __both__ constexpr vec_t()
        : x{}
        , y{}
        , z{}
        , w{} {
    }
    inline __both__ constexpr vec_t(const T &t)
        : x(t)
        , y(t)
        , z(t)
        , w(t) {
    }
    inline __both__ constexpr vec_t(const T &x, const T &y, const T &z, const T &w)
        : x(x)
        , y(y)
        , z(z)
        , w(w) {
    }
    inline __both__ constexpr vec_t(const vec_t<T, 3> &v, const T &w)
        : x(v.x)
        , y(v.y)
        , z(v.z)
        , w(w) {
    }

#ifdef __CUDACC__
    inline __both__ constexpr vec_t(const float4 &v)
        : x(v.x)
        , y(v.y)
        , z(v.z)
        , w(v.w) {
    }
    inline __both__ constexpr vec_t(const int4 &v)
        : x(v.x)
        , y(v.y)
        , z(v.z)
        , w(v.w) {
    }
    inline __both__ constexpr vec_t(const uint4 &v)
        : x(v.x)
        , y(v.y)
        , z(v.z)
        , w(v.w) {
    }
    inline __both__ operator float4() const {
        return make_float4(x, y, z, w);
    }
    inline __both__ operator int4() const {
        return make_int4(x, y, z, w);
    }
    inline __both__ operator uint4() const {
        return make_uint4(x, y, z, w);
    }
#endif

    inline __both__ vec_t &operator=(const vec_t &other) = default;

    template <typename Other>
    inline __both__ explicit constexpr vec_t(const vec_t<Other, 4> &other)
        : x((T)other.x)
        , y((T)other.y)
        , z((T)other.z)
        , w((T)other.w) {
    }

    inline __both__ constexpr T &operator[](size_t dim) {
        switch (dim) {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        default:
            return w;
        }
    }

    inline __both__ constexpr const T &operator[](size_t dim) const {
        switch (dim) {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        default:
            return w;
        }
    }

    T x, y, z, w;
};

// -------------------------------------------------------
// define vector types
// -------------------------------------------------------

#define _define_vec_types(T, t)  \
    using vec2##t = vec_t<T, 2>; \
    using vec3##t = vec_t<T, 3>; \
    using vec4##t = vec_t<T, 4>;

_define_vec_types(int8_t, c);
_define_vec_types(uint8_t, b);
_define_vec_types(int16_t, s);
_define_vec_types(uint16_t, us);
_define_vec_types(int32_t, i);
_define_vec_types(uint32_t, u);
_define_vec_types(int64_t, l);
_define_vec_types(uint64_t, ul);
_define_vec_types(float, f);
_define_vec_types(double, d);

#undef _define_vec_types

static_assert(std::is_standard_layout_v<libcbct::vec2f>);
static_assert(std::is_trivially_copyable_v<libcbct::vec2f>);
static_assert(sizeof(libcbct::vec2f) == 2 * sizeof(float));
static_assert(alignof(libcbct::vec2f) == alignof(float));

static_assert(std::is_standard_layout_v<libcbct::vec3f>);
static_assert(std::is_trivially_copyable_v<libcbct::vec3f>);
static_assert(sizeof(libcbct::vec3f) == 3 * sizeof(float));
static_assert(alignof(libcbct::vec3f) == alignof(float));

static_assert(std::is_standard_layout_v<libcbct::vec4f>);
static_assert(std::is_trivially_copyable_v<libcbct::vec4f>);
static_assert(sizeof(libcbct::vec4f) == 4 * sizeof(float));
static_assert(alignof(libcbct::vec4f) == alignof(float));

// -------------------------------------------------------
// unary functors
// -------------------------------------------------------

template <typename T>
inline __both__ T clamp(const T &val, const T &lo, const T &hi) {
    using std::max;
    using std::min;
    return min(hi, max(lo, val));
}

template <typename T>
inline __both__ T clamp(const T &val, const T &hi) {
    return clamp(val, (T)0, hi);
}

template <std::floating_point T, int N>
inline __both__ vec_t<T, N> rcp(const vec_t<T, N> &v) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = T(1.0) / v[i];
    }
    return result;
}

template <std::floating_point T, int N>
inline __both__ vec_t<T, N> sin(const vec_t<T, N> &v) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = std::sin(v[i]);
    }
    return result;
}

template <std::floating_point T, int N>
inline __both__ vec_t<T, N> cos(const vec_t<T, N> &v) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = std::cos(v[i]);
    }
    return result;
}

template <std::floating_point T, int N>
inline __both__ vec_t<T, N> tan(const vec_t<T, N> &v) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = std::tan(v[i]);
    }
    return result;
}

template <std::floating_point T, int N>
inline __both__ vec_t<T, N> sqrt(const vec_t<T, N> &v) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = std::sqrt(v[i]);
    }
    return result;
}

template <std::floating_point T, int N>
inline __both__ vec_t<T, N> saturate(const vec_t<T, N> &v) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = clamp(v[i], T(0.0), T(1.0));
    }
    return result;
}

template <typename T, int N>
inline __both__ vec_t<T, N> abs(const vec_t<T, N> &v) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = std::abs(v[i]);
    }
    return result;
}

// -------------------------------------------------------
// binary functors
// -------------------------------------------------------

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ vec_t<T, N> min(const vec_t<T, N> &a, const vec_t<T, N> &b) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = std::min(a[i], b[i]);
    }
    return result;
}

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ vec_t<T, N> max(const vec_t<T, N> &a, const vec_t<T, N> &b) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = std::max(a[i], b[i]);
    }
    return result;
}

template <std::integral T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ vec_t<T, N> divRoundUp(const vec_t<T, N> &a, const vec_t<T, N> &b) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = libcbct::divRoundUp(a[i], b[i]);
    }
    return result;
}

// -------------------------------------------------------
// unary operators
// -------------------------------------------------------

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ constexpr vec_t<T, N> operator-(const vec_t<T, N> &v) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = -v[i];
    }
    return result;
}

// -------------------------------------------------------
// binary operators
// -------------------------------------------------------

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ constexpr vec_t<T, N> operator+(const vec_t<T, N> &a, const vec_t<T, N> &b) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = a[i] + b[i];
    }
    return result;
}

template <typename T, int N, typename S>
    requires(N >= 2 && N <= 4 && std::is_arithmetic_v<S>)
inline __both__ auto operator+(const vec_t<T, N> &v, S scalar) {
    using R = typename BinaryOpResultType<T, S>::type;
    vec_t<R, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = static_cast<R>(v[i]) + static_cast<R>(scalar);
    }
    return result;
}

template <typename T, int N, typename S>
    requires(N >= 2 && N <= 4 && std::is_arithmetic_v<S>)
inline __both__ auto operator+(S scalar, const vec_t<T, N> &v) {
    return v + scalar;
}

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ constexpr vec_t<T, N> operator-(const vec_t<T, N> &a, const vec_t<T, N> &b) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = a[i] - b[i];
    }
    return result;
}

template <typename T, int N, typename S>
    requires(N >= 2 && N <= 4 && std::is_arithmetic_v<S>)
inline __both__ auto operator-(const vec_t<T, N> &v, S scalar) {
    using R = typename BinaryOpResultType<T, S>::type;
    return vec_t<R, N>(v) - vec_t<R, N>(static_cast<R>(scalar));
}

template <typename T, int N, typename S>
    requires(N >= 2 && N <= 4 && std::is_arithmetic_v<S>)
inline __both__ auto operator-(S scalar, const vec_t<T, N> &v) {
    using R = typename BinaryOpResultType<T, S>::type;
    return vec_t<R, N>(static_cast<R>(scalar)) - vec_t<R, N>(v);
}

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ constexpr vec_t<T, N> operator*(const vec_t<T, N> &a, const vec_t<T, N> &b) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = a[i] * b[i];
    }
    return result;
}

template <typename T, int N, typename S>
    requires(N >= 2 && N <= 4 && std::is_arithmetic_v<S>)
inline __both__ auto operator*(const vec_t<T, N> &v, S scalar) {
    using R = typename BinaryOpResultType<T, S>::type;
    return vec_t<R, N>(v) * vec_t<R, N>(static_cast<R>(scalar));
}

template <typename T, int N, typename S>
    requires(N >= 2 && N <= 4 && std::is_arithmetic_v<S>)
inline __both__ auto operator*(S scalar, const vec_t<T, N> &v) {
    return v * scalar;
}

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ constexpr vec_t<T, N> operator/(const vec_t<T, N> &a, const vec_t<T, N> &b) {
    vec_t<T, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = a[i] / b[i];
    }
    return result;
}

template <typename T, int N, typename S>
    requires(N >= 2 && N <= 4 && std::is_arithmetic_v<S>)
inline __both__ auto operator/(const vec_t<T, N> &v, S scalar) {
    using R = typename BinaryOpResultType<T, S>::type;
    return vec_t<R, N>(v) / vec_t<R, N>(static_cast<R>(scalar));
}

template <typename T, int N, typename S>
    requires(N >= 2 && N <= 4 && std::is_arithmetic_v<S>)
inline __both__ auto operator/(S scalar, const vec_t<T, N> &v) {
    using R = typename BinaryOpResultType<T, S>::type;
    return vec_t<R, N>(static_cast<R>(scalar)) / vec_t<R, N>(v);
}

// -------------------------------------------------------
// compare operators
// -------------------------------------------------------

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ constexpr bool operator==(const vec_t<T, N> &a, const vec_t<T, N> &b) {
    for (int i = 0; i < N; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ constexpr bool operator!=(const vec_t<T, N> &a, const vec_t<T, N> &b) {
    return !(a == b);
}

// -------------------------------------------------------
// assign operators
// -------------------------------------------------------

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ vec_t<T, N> &operator+=(vec_t<T, N> &a, const vec_t<T, N> &b) {
    a = a + b;
    return a;
}

template <typename T, int N, typename S>
    requires(N >= 2 && N <= 4 && std::is_arithmetic_v<S>)
inline __both__ vec_t<T, N> &operator+=(vec_t<T, N> &a, S scalar) {
    for (int i = 0; i < N; ++i) {
        a[i] += scalar;
    }
    return a;
}

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ vec_t<T, N> &operator-=(vec_t<T, N> &a, const vec_t<T, N> &b) {
    a = a - b;
    return a;
}

template <typename T, int N, typename S>
    requires(N >= 2 && N <= 4 && std::is_arithmetic_v<S>)
inline __both__ vec_t<T, N> &operator-=(vec_t<T, N> &a, S scalar) {
    for (int i = 0; i < N; ++i) {
        a[i] -= scalar;
    }
    return a;
}

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ vec_t<T, N> &operator*=(vec_t<T, N> &a, const vec_t<T, N> &b) {
    a = a * b;
    return a;
}

template <typename T, int N, typename S>
    requires(N >= 2 && N <= 4 && std::is_arithmetic_v<S>)
inline __both__ vec_t<T, N> &operator*=(vec_t<T, N> &a, S scalar) {
    for (int i = 0; i < N; ++i) {
        a[i] *= scalar;
    }
    return a;
}

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ vec_t<T, N> &operator/=(vec_t<T, N> &a, const vec_t<T, N> &b) {
    a = a / b;
    return a;
}

template <typename T, int N, typename S>
    requires(N >= 2 && N <= 4 && std::is_arithmetic_v<S>)
inline __both__ vec_t<T, N> &operator/=(vec_t<T, N> &a, S scalar) {
    for (int i = 0; i < N; ++i) {
        a[i] /= scalar;
    }
    return a;
}

// -------------------------------------------------------
// GLSL-like functors
// -------------------------------------------------------

template <std::floating_point T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ T length(const vec_t<T, N> &v) {
    return std::sqrt(dot(v, v));
}

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ T dot(const vec_t<T, N> &a, const vec_t<T, N> &b) {
    T result = a[0] * b[0];
    for (int i = 1; i < N; ++i) {
        result += a[i] * b[i];
    }
    return result;
}

template <typename T>
inline __both__ vec_t<T, 3> cross(const vec_t<T, 3> &a, const vec_t<T, 3> &b) {
    return vec_t<T, 3>(a.y * b.z - b.y * a.z, a.z * b.x - b.z * a.x, a.x * b.y - b.x * a.y);
}

template <std::floating_point T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ vec_t<T, N> normalize(const vec_t<T, N> &v) {
    return v / length(v);
}

}  // namespace libcbct

#endif  // LIBCBCT_VEC_H
