#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_LOGGING_H
#define LIBCBCT_LOGGING_H

#include <iostream>
#include <memory>
#include <stdexcept>

// -----------------------------------------------------------------------------
// ANSI color
// -----------------------------------------------------------------------------
#define FG_RED "\033[31m"
#define FG_GRN "\033[32m"
#define FG_YEL "\033[33m"
#define FG_BLU "\033[34m"
#define FG_NC "\033[0m"

// -----------------------------------------------------------------------------
// Message handlers
// -----------------------------------------------------------------------------

template <typename... Args>
std::string STR_FMT(const char *format, Args... args) {
    int size_s = std::snprintf(nullptr, 0, format, args...) + 1;
    if (size_s <= 0) {
        throw std::runtime_error("Error during formatting.");
    }
    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, format, args...);
    return std::string(buf.get(), buf.get() + size - 1);
}

template <typename... Args>
void LIBCBCT_INFO(const char *format, Args... args) {
    std::cout << FG_BLU << "[INFO] " << FG_NC << STR_FMT(format, args...) << std::endl;
}

template <typename... Args>
void LIBCBCT_DEBUG(const char *format, Args... args) {
    std::cout << FG_GRN << "[DEBUG] " << FG_NC << STR_FMT(format, args...) << std::endl;
}

template <typename... Args>
void LIBCBCT_WARN(const char *format, Args... args) {
    std::cout << FG_YEL << "[WARNING] " << FG_NC << STR_FMT(format, args...) << std::endl;
}

template <typename... Args>
void LIBCBCT_ERROR(const char *format, Args... args) {
    throw std::runtime_error(STR_FMT(format, args...));
}

// -----------------------------------------------------------------------------
// Assertion with message
// -----------------------------------------------------------------------------

#ifndef __FUNCTION_NAME__
#    if defined(_WIN32) || defined(__WIN32__)
#        define __FUNCTION_NAME__ __FUNCTION__
#    else
#        define __FUNCTION_NAME__ __func__
#    endif
#endif

inline std::string ASRT_LABEL(const char *predicate, const char *file, int line, const char *funcname) {
    return STR_FMT("Assertion \"%s\" failed in %s L-%d (%s)", predicate, file, line, funcname);
}

#if !defined(LIBCBCT_DISABLE_ASSERT)
#    define LIBCBCT_ASSERT(PREDICATE, ...)                                                                 \
        do {                                                                                             \
            if (!(PREDICATE)) {                                                                          \
                std::cerr << ASRT_LABEL(#PREDICATE, __FILE__, __LINE__, __FUNCTION_NAME__) << std::endl; \
                std::abort();                                                                            \
            }                                                                                            \
        } while (false)
#else        // LIBCBCT_DISABLE_ASSERT
#    define LIBCBCT_ASSERT(PREDICATE, ...) \
        do {                             \
        } while (false)
#endif        // LIBCBCT_DISABLE_ASSERT

#endif  // LIBCBCT_LOGGING_H
