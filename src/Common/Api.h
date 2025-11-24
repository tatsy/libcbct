#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_API_H
#define LIBCBCT_API_H

// -----------------------------------------------------------------------------
// API export macro
// -----------------------------------------------------------------------------

#if (defined(WIN32) || defined(_WIN32) || defined(WINCE) || defined(__CYGWIN__))
#if defined(LIBCBCT_API_EXPORT)
#define LIBCBCT_API __declspec(dllexport)
#else
#define LIBCBCT_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#define LIBCBCT_API __attribute__((visibility("default")))
#else
#define LIBCBCT_API
#endif

#if defined(_MSC_VER)
#if !defined(LIBCBCT_NOINLINE)
#define LIBCBCT_NOINLINE __declspec(noinline)
#endif
#if !defined(LIBCBCT_FORCEINLINE)
#define LIBCBCT_FORCEINLINE __forceinline
#endif
#elif defined(__GNUC__) || defined(__clang__)
#if !defined(LIBCBCT_NOINLINE)
#define LIBCBCT_NOINLINE __attribute__((noinline))
#endif
#if !defined(LIBCBCT_FORCEINLINE)
#define LIBCBCT_FORCEINLINE inline __attribute__((always_inline))
#endif
#else
#if !defined(LIBCBCT_NOINLINE)
#define LIBCBCT_NOINLINE
#endif
#if !defined(LIBCBCT_FORCEINLINE)
#define LIBCBCT_FORCEINLINE inline
#endif
#endif

#ifdef __CUDACC__
#define LIBCBCT_HOST_DEVICE __host__ __device__ __forceinline__
#else
#define LIBCBCT_HOST_DEVICE LIBCBCT_FORCEINLINE
#endif

#endif  // LIBCBCT_API_H
