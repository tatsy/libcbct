#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_VEC_H
#define LIBCBCT_VEC_H

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <algorithm>
#include <type_traits>

#ifdef __CUDACC__
#define __both__ __host__ __device__
#else
#define __both__
#endif

namespace libcbct {

// Default vector type template
template <typename T, int N>
struct vec_t {
    T t[N];
};

// Binary Result type: scalar type with itself always returns same type
template <typename ScalarTypeA, typename ScalarTypeB>
struct BinaryOpResultType {
    using type = std::common_type_t<ScalarTypeA, ScalarTypeB>;
};

template <typename ScalarType>
struct BinaryOpResultType<ScalarType, ScalarType> {
    typedef ScalarType type;
};

template <>
struct BinaryOpResultType<int, float> {
    typedef float type;
};
template <>
struct BinaryOpResultType<float, int> {
    typedef float type;
};
template <>
struct BinaryOpResultType<unsigned int, float> {
    typedef float type;
};
template <>
struct BinaryOpResultType<float, unsigned int> {
    typedef float type;
};

template <>
struct BinaryOpResultType<int, double> {
    typedef double type;
};
template <>
struct BinaryOpResultType<double, int> {
    typedef double type;
};
template <>
struct BinaryOpResultType<unsigned int, double> {
    typedef double type;
};
template <>
struct BinaryOpResultType<double, unsigned int> {
    typedef double type;
};

inline __both__ int32_t divRoundUp(int32_t a, int32_t b) {
    return (a + b - 1) / b;
}
inline __both__ uint32_t divRoundUp(uint32_t a, uint32_t b) {
    return (a + b - 1) / b;
}
inline __both__ int64_t divRoundUp(int64_t a, int64_t b) {
    return (a + b - 1) / b;
}
inline __both__ uint64_t divRoundUp(uint64_t a, uint64_t b) {
    return (a + b - 1) / b;
}

// -----------------------------------------------------------------------------
// vec2
// -----------------------------------------------------------------------------

template <typename T>
struct vec_t<T, 2> {
    enum { dims = 2 };
    typedef T scalar_t;

    inline __both__ vec_t()
        : x{}
        , y{} {
    }

    inline __both__ vec_t(const T &t)
        : x(t)
        , y(t) {
    }
    inline __both__ constexpr vec_t(const T &x, const T &y)
        : x(x)
        , y(y) {
    }

#ifdef __CUDACC__
    inline __both__ vec_t(const float2 &v)
        : x(v.x)
        , y(v.y) {
    }
    inline __both__ vec_t(const int2 &v)
        : x(v.x)
        , y(v.y) {
    }
    inline __both__ vec_t(const uint2 &v)
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
    inline __both__ explicit vec_t(const vec_t<Other, 2> &other)
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

    union {
        struct {
            T x, y;
        };
        struct {
            T s, t;
        };
        struct {
            T u, v;
        };
    };
};

// -----------------------------------------------------------------------------
// vec3
// -----------------------------------------------------------------------------

template <typename T>
struct vec_t<T, 3> {
    enum { dims = 3 };
    typedef T scalar_t;

    inline __both__ vec_t()
        : x{}
        , y{}
        , z{} {
    }

    inline __both__ vec_t(const T &t)
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
    inline __both__ vec_t(const float3 &v)
        : x(v.x)
        , y(v.y)
        , z(v.z) {
    }
    inline __both__ vec_t(const int3 &v)
        : x(v.x)
        , y(v.y)
        , z(v.z) {
    }
    inline __both__ vec_t(const uint3 &v)
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
    inline __both__ explicit vec_t(const vec_t<Other, 3> &other)
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

    union {
        struct {
            T x, y, z;
        };
        struct {
            T r, g, b;
        };
        struct {
            T u, v, w;
        };
    };
};

// -----------------------------------------------------------------------------
// vec4
// -----------------------------------------------------------------------------

template <typename T>
struct vec_t<T, 4> {
    enum { dims = 4 };
    typedef T scalar_t;

    inline __both__ vec_t()
        : x{}
        , y{}
        , z{}
        , w{} {
    }
    inline __both__ vec_t(const T &t)
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
    inline __both__ vec_t(const vec_t<T, 3> &v, const T &w)
        : x(v.x)
        , y(v.y)
        , z(v.z)
        , w(w) {
    }

#ifdef __CUDACC__
    inline __both__ vec_t(const float4 &v)
        : x(v.x)
        , y(v.y)
        , z(v.z)
        , w(v.w) {
    }
    inline __both__ vec_t(const int4 &v)
        : x(v.x)
        , y(v.y)
        , z(v.z)
        , w(v.w) {
    }
    inline __both__ vec_t(const uint4 &v)
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
    inline __both__ explicit vec_t(const vec_t<Other, 4> &other)
        : x((T)other.x)
        , y((T)other.y)
        , z((T)other.z)
        , w((T)other.w) {
    }

    inline __both__ constexpr T &operator[](size_t dim) {
        static_assert(dim < 4, "Index out of bounds for vec4");
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

    union {
        struct {
            T x, y, z, w;
        };
        struct {
            T r, g, b, a;
        };
    };
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

#define _define_float_functor(func, scalarFunc)                                                 \
    template <typename T>                                                                       \
    inline __both__ vec_t<T, 2> func(const vec_t<T, 2> &v) {                                    \
        return vec_t<T, 2>(scalarFunc(v.x), scalarFunc(v.y));                                   \
    }                                                                                           \
                                                                                                \
    template <typename T>                                                                       \
    inline __both__ vec_t<T, 3> func(const vec_t<T, 3> &v) {                                    \
        return vec_t<T, 3>(scalarFunc(v.x), scalarFunc(v.y), scalarFunc(v.z));                  \
    }                                                                                           \
                                                                                                \
    template <typename T>                                                                       \
    inline __both__ vec_t<T, 4> func(const vec_t<T, 4> &v) {                                    \
        return vec_t<T, 4>(scalarFunc(v.x), scalarFunc(v.y), scalarFunc(v.z), scalarFunc(v.w)); \
    }

// clang-format off
_define_float_functor(rcp, rcp)
_define_float_functor(sin, std::sin)
_define_float_functor(cos, std::cos)
_define_float_functor(abs, std::abs)
_define_float_functor(sqrt, std::sqrt)
_define_float_functor(saturate, saturate)
// clang-format on

#undef _define_float_functor

// -------------------------------------------------------
// binary functors
// -------------------------------------------------------

#define _define_binary_functor(fct, scalarFunc)                                                                      \
    template <typename T>                                                                                            \
    __both__ vec_t<T, 1> fct(const vec_t<T, 1> &a, const vec_t<T, 1> &b) {                                           \
        return vec_t<T, 1>(scalarFunc(a.x, b.x));                                                                    \
    }                                                                                                                \
                                                                                                                     \
    template <typename T>                                                                                            \
    __both__ vec_t<T, 2> fct(const vec_t<T, 2> &a, const vec_t<T, 2> &b) {                                           \
        return vec_t<T, 2>(scalarFunc(a.x, b.x), scalarFunc(a.y, b.y));                                              \
    }                                                                                                                \
                                                                                                                     \
    template <typename T>                                                                                            \
    __both__ vec_t<T, 3> fct(const vec_t<T, 3> &a, const vec_t<T, 3> &b) {                                           \
        return vec_t<T, 3>(scalarFunc(a.x, b.x), scalarFunc(a.y, b.y), scalarFunc(a.z, b.z));                        \
    }                                                                                                                \
                                                                                                                     \
    template <typename T1, typename T2>                                                                              \
    __both__ vec_t<typename BinaryOpResultType<T1, T2>::type, 3> fct(const vec_t<T1, 3> &a, const vec_t<T2, 3> &b) { \
        return vec_t<typename BinaryOpResultType<T1, T2>::type, 3>(scalarFunc(a.x, b.x), scalarFunc(a.y, b.y),       \
                                                                   scalarFunc(a.z, b.z));                            \
    }                                                                                                                \
                                                                                                                     \
    template <typename T>                                                                                            \
    __both__ vec_t<T, 4> fct(const vec_t<T, 4> &a, const vec_t<T, 4> &b) {                                           \
        return vec_t<T, 4>(scalarFunc(a.x, b.x), scalarFunc(a.y, b.y), scalarFunc(a.z, b.z), scalarFunc(a.w, b.w));  \
    }

    // clang-format off
_define_binary_functor(divRoundUp, libcbct::divRoundUp)
_define_binary_functor(min, std::min)
_define_binary_functor(max, std::max)
// clang-format on

#undef _define_binary_functor

    // -------------------------------------------------------
    // unary operators
    // -------------------------------------------------------

    template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ vec_t<T, N> operator-(const vec_t<T, N> &v) {
    vec_t<T, N> result;
    result[0] = -v[0];
    result[1] = -v[1];
    if constexpr (N == 2) {
        return result;
    }

    result[2] = -v[2];
    if constexpr (N == 3) {
        return result;
    }

    result[3] = -v[3];
    return result;
}

// -------------------------------------------------------
// binary operators
// -------------------------------------------------------

template <typename T, int N>
    requires(N >= 2 && N <= 4)
constexpr vec_t<T, N> operator+(const vec_t<T, N> &a, const vec_t<T, N> &b) {
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
    return vec_t<R, N>(v) + vec_t<R, N>(static_cast<R>(scalar));
}

template <typename T, int N>
    requires(N >= 2 && N <= 4)
constexpr vec_t<T, N> operator-(const vec_t<T, N> &a, const vec_t<T, N> &b) {
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

template <typename T, int N>
    requires(N >= 2 && N <= 4)
constexpr vec_t<T, N> operator*(const vec_t<T, N> &a, const vec_t<T, N> &b) {
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

template <typename T, int N>
    requires(N >= 2 && N <= 4)
constexpr vec_t<T, N> operator/(const vec_t<T, N> &a, const vec_t<T, N> &b) {
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

// -------------------------------------------------------
// compare operators
// -------------------------------------------------------

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ bool operator==(const vec_t<T, N> &a, const vec_t<T, N> &b) {
    if (a[0] != b[0]) return false;
    if (a[1] != b[1]) return false;
    if constexpr (N == 2) {
        return true;
    }

    if (a[2] != b[2]) return false;
    if constexpr (N == 3) {
        return true;
    }

    if (a[3] != b[3]) return false;
    return true;
}

template <typename T, int N>
    requires(N >= 2 && N <= 4)
inline __both__ bool operator!=(const vec_t<T, N> &a, const vec_t<T, N> &b) {
    return !(a == b);
}

// -------------------------------------------------------
// assign operators
// -------------------------------------------------------

template <typename T, int N>
inline __both__ vec_t<T, N> &operator+=(vec_t<T, N> &a, const vec_t<T, N> &b) {
    a = a + b;
    return a;
}

template <typename T, int N, typename S>
inline __both__ vec_t<T, N> &operator+=(vec_t<T, N> &a, S scalar) {
    a = a + scalar;
    return a;
}

template <typename T, int N>
inline __both__ vec_t<T, N> &operator-=(vec_t<T, N> &a, const vec_t<T, N> &b) {
    a = a - b;
    return a;
}

template <typename T, int N, typename S>
inline __both__ vec_t<T, N> &operator-=(vec_t<T, N> &a, S scalar) {
    a = a - scalar;
    return a;
}

template <typename T, int N>
inline __both__ vec_t<T, N> &operator*=(vec_t<T, N> &a, const vec_t<T, N> &b) {
    a = a * b;
    return a;
}

template <typename T, int N, typename S>
inline __both__ vec_t<T, N> &operator*=(vec_t<T, N> &a, S scalar) {
    a = a * scalar;
    return a;
}

template <typename T, int N>
inline __both__ vec_t<T, N> &operator/=(vec_t<T, N> &a, const vec_t<T, N> &b) {
    a = a / b;
    return a;
}

template <typename T, int N, typename S>
inline __both__ vec_t<T, N> &operator/=(vec_t<T, N> &a, S scalar) {
    a = a / scalar;
    return a;
}

// -------------------------------------------------------
// GLSL-like functors
// -------------------------------------------------------

template <typename T>
inline __both__ T length(const vec_t<T, 3> &v) {
    return sqrtf(dot(v, v));
}

template <typename T>
inline __both__ T dot(const vec_t<T, 3> &a, const vec_t<T, 3> &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <typename T>
inline __both__ vec_t<T, 3> cross(const vec_t<T, 3> &a, const vec_t<T, 3> &b) {
    return vec_t<T, 3>(a.y * b.z - b.y * a.z, a.z * b.x - b.z * a.x, a.x * b.y - b.x * a.y);
}

template <typename T>
inline __both__ vec_t<T, 3> normalize(const vec_t<T, 3> &v) {
    return v * 1.0f / length(v);
}

}  // namespace libcbct

#endif  // LIBCBCT_VEC_H
