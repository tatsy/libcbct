#pragma once

#include <math.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __CUDACC__
#define __both__ __host__ __device__
#else
#define __both__
#endif

// Default vector type template
template <typename T, int N>
struct vec_t {
    T t[N];
};

// Binary Result type: scalar type with itself always returns same type
template <typename ScalarTypeA, typename ScalarTypeB>
struct BinaryOpResultType;
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

    inline __both__ vec_t() {
    }
    inline __both__ vec_t(const T &t)
        : x(t)
        , y(t) {
    }
    inline __both__ vec_t(const T &x, const T &y)
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

    inline __both__ vec_t<T, 2> &operator=(const vec_t<T, 2> &other) {
        this->x = other.x;
        this->y = other.y;
        return *this;
    }

    inline __both__ vec_t<T, 2> &operator+=(const vec_t<T, 2> &other);
    inline __both__ vec_t<T, 2> &operator-=(const vec_t<T, 2> &other);
    inline __both__ vec_t<T, 2> &operator*=(const vec_t<T, 2> &other);
    inline __both__ vec_t<T, 2> &operator/=(const vec_t<T, 2> &other);

    inline __both__ vec_t<T, 2> &operator+=(const T &other);
    inline __both__ vec_t<T, 2> &operator-=(const T &other);
    inline __both__ vec_t<T, 2> &operator*=(const T &other);
    inline __both__ vec_t<T, 2> &operator/=(const T &other);

    template <typename Other>
    inline __both__ explicit vec_t(const vec_t<Other, 2> &other)
        : x((T)other.x)
        , y((T)other.y) {
    }

    inline __both__ T &operator[](size_t dim) {
        return (&x)[dim];
    }
    inline __both__ const T &operator[](size_t dim) const {
        return (&x)[dim];
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

    inline __both__ vec_t() {
    }
    inline __both__ vec_t(const T &t)
        : x(t)
        , y(t)
        , z(t) {
    }
    inline __both__ vec_t(const T &x, const T &y, const T &z)
        : x(x)
        , y(y)
        , z(z) {
    }
    inline __both__ vec_t(const vec_t<T, 4> &v);

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

    inline __both__ vec_t<T, 3> &operator=(const vec_t<T, 3> &other) {
        this->x = other.x;
        this->y = other.y;
        this->z = other.z;
        return *this;
    }

    inline __both__ vec_t<T, 3> &operator+=(const vec_t<T, 3> &other);
    inline __both__ vec_t<T, 3> &operator-=(const vec_t<T, 3> &other);
    inline __both__ vec_t<T, 3> &operator*=(const vec_t<T, 3> &other);
    inline __both__ vec_t<T, 3> &operator/=(const vec_t<T, 3> &other);

    inline __both__ vec_t<T, 3> &operator+=(const T &other);
    inline __both__ vec_t<T, 3> &operator-=(const T &other);
    inline __both__ vec_t<T, 3> &operator*=(const T &other);
    inline __both__ vec_t<T, 3> &operator/=(const T &other);

    template <typename Other>
    inline __both__ explicit vec_t(const vec_t<Other, 3> &other)
        : x((T)other.x)
        , y((T)other.y)
        , z((T)other.z) {
    }

    inline __both__ T &operator[](size_t dim) {
        return (&x)[dim];
    }
    inline __both__ const T &operator[](size_t dim) const {
        return (&x)[dim];
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

    inline __both__ vec_t() {
    }
    inline __both__ vec_t(const T &t)
        : x(t)
        , y(t)
        , z(t)
        , w(t) {
    }
    inline __both__ vec_t(const T &x, const T &y, const T &z, const T &w)
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

    inline __both__ vec_t<T, 3> &operator=(const vec_t<T, 3> &other) {
        this->x = other.x;
        this->y = other.y;
        this->z = other.z;
        this->w = other.w;
        return *this;
    }

    inline __both__ vec_t<T, 4> &operator+=(const vec_t<T, 4> &other);
    inline __both__ vec_t<T, 4> &operator-=(const vec_t<T, 4> &other);
    inline __both__ vec_t<T, 4> &operator*=(const vec_t<T, 4> &other);
    inline __both__ vec_t<T, 4> &operator/=(const vec_t<T, 4> &other);

    inline __both__ vec_t<T, 4> &operator+=(const T &other);
    inline __both__ vec_t<T, 4> &operator-=(const T &other);
    inline __both__ vec_t<T, 4> &operator*=(const T &other);
    inline __both__ vec_t<T, 4> &operator/=(const T &other);

    template <typename Other>
    inline __both__ explicit vec_t(const vec_t<Other, 4> &other)
        : x((T)other.x)
        , y((T)other.y)
        , z((T)other.z)
        , w((T)other.w) {
    }

    inline __both__ T &operator[](size_t dim) {
        return (&x)[dim];
    }
    inline __both__ const T &operator[](size_t dim) const {
        return (&x)[dim];
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

template <typename T>
inline __both__ vec_t<T, 3>::vec_t(const vec_t<T, 4> &v)
    : x(v.x)
    , y(v.y)
    , z(v.z) {
}

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
    return min(hi, max(lo, val));
}

template <typename T>
inline __both__ T clamp(const T &val, const T &hi) {
    return clamp(val, (T)0, hi);
}

#define _define_float_functor(func)                                     \
    template <typename T>                                               \
    inline __both__ vec_t<T, 2> func(const vec_t<T, 2> &v) {            \
        return vec_t<T, 2>(func(v.x), func(v.y));                       \
    }                                                                   \
                                                                        \
    template <typename T>                                               \
    inline __both__ vec_t<T, 3> func(const vec_t<T, 3> &v) {            \
        return vec_t<T, 3>(func(v.x), func(v.y), func(v.z));            \
    }                                                                   \
                                                                        \
    template <typename T>                                               \
    inline __both__ vec_t<T, 4> func(const vec_t<T, 4> &v) {            \
        return vec_t<T, 4>(func(v.x), func(v.y), func(v.z), func(v.w)); \
    }

// clang-format off
_define_float_functor(rcp)
_define_float_functor(sin)
_define_float_functor(cos)
_define_float_functor(abs)
_define_float_functor(sqrt)
_define_float_functor(saturate)
// clang-format on

#undef _define_float_functor

// -------------------------------------------------------
// binary functors
// -------------------------------------------------------

#define _define_binary_functor(fct)                                                                                  \
    template <typename T>                                                                                            \
    __both__ vec_t<T, 1> fct(const vec_t<T, 1> &a, const vec_t<T, 1> &b) {                                           \
        return vec_t<T, 1>(fct(a.x, b.x));                                                                           \
    }                                                                                                                \
                                                                                                                     \
    template <typename T>                                                                                            \
    __both__ vec_t<T, 2> fct(const vec_t<T, 2> &a, const vec_t<T, 2> &b) {                                           \
        return vec_t<T, 2>(fct(a.x, b.x), fct(a.y, b.y));                                                            \
    }                                                                                                                \
                                                                                                                     \
    template <typename T>                                                                                            \
    __both__ vec_t<T, 3> fct(const vec_t<T, 3> &a, const vec_t<T, 3> &b) {                                           \
        return vec_t<T, 3>(fct(a.x, b.x), fct(a.y, b.y), fct(a.z, b.z));                                             \
    }                                                                                                                \
                                                                                                                     \
    template <typename T1, typename T2>                                                                              \
    __both__ vec_t<typename BinaryOpResultType<T1, T2>::type, 3> fct(const vec_t<T1, 3> &a, const vec_t<T2, 3> &b) { \
        return vec_t<typename BinaryOpResultType<T1, T2>::type, 3>(fct(a.x, b.x), fct(a.y, b.y), fct(a.z, b.z));     \
    }                                                                                                                \
                                                                                                                     \
    template <typename T>                                                                                            \
    __both__ vec_t<T, 4> fct(const vec_t<T, 4> &a, const vec_t<T, 4> &b) {                                           \
        return vec_t<T, 4>(fct(a.x, b.x), fct(a.y, b.y), fct(a.z, b.z), fct(a.w, b.w));                              \
    }

    // clang-format off
_define_binary_functor(divRoundUp)
_define_binary_functor(min)
_define_binary_functor(max)
// clang-format on

#undef _define_binary_functor

// -------------------------------------------------------
// unary operators
// -------------------------------------------------------

#define _define_unary_op(op)                                        \
    template <typename T>                                           \
    inline __both__ vec_t<T, 2> operator op(const vec_t<T, 2> &v) { \
        return vec_t<T, 2>(op v.x, op v.y);                         \
    }                                                               \
                                                                    \
    template <typename T>                                           \
    inline __both__ vec_t<T, 3> operator op(const vec_t<T, 3> &v) { \
        return vec_t<T, 3>(op v.x, op v.y, op v.z);                 \
    }                                                               \
                                                                    \
    template <typename T>                                           \
    inline __both__ vec_t<T, 4> operator op(const vec_t<T, 4> &v) { \
        return vec_t<T, 4>(op v.x, op v.y, op v.z, op v.w);         \
    }

        _define_unary_op(+);
_define_unary_op(-);

// -------------------------------------------------------
// binary operators
// -------------------------------------------------------

#define _define_operator(op)                                                                               \
    /* vec op vec */                                                                                       \
    template <typename T>                                                                                  \
    inline __both__ vec_t<T, 2> operator op(const vec_t<T, 2> &a, const vec_t<T, 2> &b) {                  \
        return vec_t<T, 2>(a.x op b.x, a.y op b.y);                                                        \
    }                                                                                                      \
                                                                                                           \
    template <typename T>                                                                                  \
    inline __both__ vec_t<T, 3> operator op(const vec_t<T, 3> &a, const vec_t<T, 3> &b) {                  \
        return vec_t<T, 3>(a.x op b.x, a.y op b.y, a.z op b.z);                                            \
    }                                                                                                      \
                                                                                                           \
    template <typename T>                                                                                  \
    inline __both__ vec_t<T, 4> operator op(const vec_t<T, 4> &a, const vec_t<T, 4> &b) {                  \
        return vec_t<T, 4>(a.x op b.x, a.y op b.y, a.z op b.z, a.w op b.w);                                \
    }                                                                                                      \
                                                                                                           \
    /* vec op scalar */                                                                                    \
    template <typename T>                                                                                  \
    inline __both__ vec_t<T, 2> operator op(const vec_t<T, 2> &a, const T &b) {                            \
        return vec_t<T, 2>(a.x op b, a.y op b);                                                            \
    }                                                                                                      \
                                                                                                           \
    template <typename T1, typename T2>                                                                    \
    inline __both__ vec_t<typename BinaryOpResultType<T1, T2>::type, 3> operator op(const vec_t<T1, 3> &a, \
                                                                                    const T2 &b) {         \
        return vec_t<typename BinaryOpResultType<T1, T2>::type, 3>(a.x op b, a.y op b, a.z op b);          \
    }                                                                                                      \
                                                                                                           \
    template <typename T>                                                                                  \
    inline __both__ vec_t<T, 4> operator op(const vec_t<T, 4> &a, const T &b) {                            \
        return vec_t<T, 4>(a.x op b, a.y op b, a.z op b, a.w op b);                                        \
    }                                                                                                      \
                                                                                                           \
    /* scalar op vec */                                                                                    \
    template <typename T>                                                                                  \
    inline __both__ vec_t<T, 2> operator op(const T &a, const vec_t<T, 2> &b) {                            \
        return vec_t<T, 2>(a op b.x, a op b.y);                                                            \
    }                                                                                                      \
                                                                                                           \
    template <typename T>                                                                                  \
    inline __both__ vec_t<T, 3> operator op(const T &a, const vec_t<T, 3> &b) {                            \
        return vec_t<T, 3>(a op b.x, a op b.y, a op b.z);                                                  \
    }                                                                                                      \
                                                                                                           \
    template <typename T>                                                                                  \
    inline __both__ vec_t<T, 4> operator op(const T &a, const vec_t<T, 4> &b) {                            \
        return vec_t<T, 4>(a op b.x, a op b.y, a op b.z, a op b.w);                                        \
    }

_define_operator(*);
_define_operator(/);
_define_operator(+);
_define_operator(-);
#undef _define_operator

// -------------------------------------------------------
// compare operators
// -------------------------------------------------------

#define _define_compare_operator(op)                                               \
    template <typename T>                                                          \
    inline __both__ bool operator op(const vec_t<T, 2> &a, const vec_t<T, 2> &b) { \
        return a.x op b.x && a.y op b.y;                                           \
    }                                                                              \
                                                                                   \
    template <typename T>                                                          \
    inline __both__ bool operator op(const vec_t<T, 3> &a, const vec_t<T, 3> &b) { \
        return a.x op b.x && a.y op b.y && a.z op b.z;                             \
    }                                                                              \
                                                                                   \
    template <typename T>                                                          \
    inline __both__ bool operator op(const vec_t<T, 4> &a, const vec_t<T, 4> &b) { \
        return a.x op b.x && a.y op b.y && a.z op b.z && a.w op b.w;               \
    }

_define_compare_operator(<);
_define_compare_operator(>);
_define_compare_operator(==);
_define_compare_operator(!=);

// -------------------------------------------------------
// assign operators
// -------------------------------------------------------

#define _define_assign_operator(op)                                                    \
    /* vec op vec */                                                                   \
    template <typename T>                                                              \
    inline __both__ vec_t<T, 2> &vec_t<T, 2>::operator op## = (const vec_t<T, 2> &o) { \
        *this = *this op o;                                                            \
        return *this;                                                                  \
    }                                                                                  \
                                                                                       \
    template <typename T>                                                              \
    inline __both__ vec_t<T, 3> &vec_t<T, 3>::operator op## = (const vec_t<T, 3> &o) { \
        *this = *this op o;                                                            \
        return *this;                                                                  \
    }                                                                                  \
                                                                                       \
    template <typename T>                                                              \
    inline __both__ vec_t<T, 4> &vec_t<T, 4>::operator op## = (const vec_t<T, 4> &o) { \
        *this = *this op o;                                                            \
        return *this;                                                                  \
    }                                                                                  \
                                                                                       \
    /* vec op scalar */                                                                \
    template <typename T>                                                              \
    inline __both__ vec_t<T, 2> &vec_t<T, 2>::operator op## = (const T &o) {           \
        *this = *this op o;                                                            \
        return *this;                                                                  \
    }                                                                                  \
                                                                                       \
    template <typename T>                                                              \
    inline __both__ vec_t<T, 3> &vec_t<T, 3>::operator op## = (const T &o) {           \
        *this = *this op o;                                                            \
        return *this;                                                                  \
    }                                                                                  \
                                                                                       \
    template <typename T>                                                              \
    inline __both__ vec_t<T, 4> &vec_t<T, 4>::operator op## = (const T &o) {           \
        *this = *this op o;                                                            \
        return *this;                                                                  \
    }

_define_assign_operator(*);
_define_assign_operator(/);
_define_assign_operator(+);
_define_assign_operator(-);
#undef _define_assign_operator

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
