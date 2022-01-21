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
#define LIBCBCT_API
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#define LIBCBCT_API __attribute__((visibility("default")))
#else
#define LIBCBCT_API
#endif

#endif  // LIBCBCT_API_H
