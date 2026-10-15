#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_LOGGING_H
#define LIBCBCT_LOGGING_H

#include <format>
#include <iostream>
#include <string>
#include <utility>

#include "Exception.h"

// -----------------------------------------------------------------------------
// ANSI color
// -----------------------------------------------------------------------------
#define FG_RED "\033[31m"
#define FG_GRN "\033[32m"
#define FG_YEL "\033[33m"
#define FG_BLU "\033[34m"
#define FG_NC "\033[0m"

namespace libcbct {

// -----------------------------------------------------------------------------
// Message handlers
// -----------------------------------------------------------------------------
// Format strings use std::format syntax ("{}", "{:f}", ...). Taking
// std::format_string makes the compiler validate the format string against the
// argument types, so mismatches and non-literal format strings are rejected at
// compile time.

template <typename... Args>
std::string STR_FMT(std::format_string<Args...> fmt, Args &&...args) {
    return std::format(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void LIBCBCT_INFO(std::format_string<Args...> fmt, Args &&...args) {
    std::cout << FG_BLU << "[INFO] " << FG_NC << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
void LIBCBCT_DEBUG(std::format_string<Args...> fmt, Args &&...args) {
    std::cout << FG_GRN << "[DEBUG] " << FG_NC << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
void LIBCBCT_WARN(std::format_string<Args...> fmt, Args &&...args) {
    std::cout << FG_YEL << "[WARNING] " << FG_NC << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename E = libcbct::Error, typename... Args>
[[noreturn]] void LIBCBCT_THROW(std::format_string<Args...> fmt, Args &&...args) {
    throw E(std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void LIBCBCT_ERROR(std::format_string<Args...> fmt, Args &&...args) {
    LIBCBCT_THROW<libcbct::Error>(fmt, std::forward<Args>(args)...);
}

// -----------------------------------------------------------------------------
// Assertion with message
// -----------------------------------------------------------------------------

#ifndef __FUNCTION_NAME__
#if defined(_WIN32) || defined(__WIN32__)
#define __FUNCTION_NAME__ __FUNCTION__
#else
#define __FUNCTION_NAME__ __func__
#endif
#endif

inline std::string ASRT_LABEL(const char *predicate, const char *file, int line, const char *funcname) {
    return STR_FMT("Assertion \"{}\" failed in {} L-{} ({})", predicate, file, line, funcname);
}

#if !defined(LIBCBCT_DISABLE_ASSERT)
#define LIBCBCT_ASSERT(PREDICATE, ...)                                                                          \
    do {                                                                                                        \
        if (!(PREDICATE)) {                                                                                     \
            std::cerr << ::libcbct::ASRT_LABEL(#PREDICATE, __FILE__, __LINE__, __FUNCTION_NAME__) << std::endl; \
            std::abort();                                                                                       \
        }                                                                                                       \
    } while (false)
#else  // LIBCBCT_DISABLE_ASSERT
#define LIBCBCT_ASSERT(PREDICATE, ...) \
    do {                               \
    } while (false)
#endif  // LIBCBCT_DISABLE_ASSERT

}  // namespace libcbct

#endif  // LIBCBCT_LOGGING_H
