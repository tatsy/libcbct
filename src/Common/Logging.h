#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_LOGGING_H
#define LIBCBCT_LOGGING_H

#include <cstdlib>
#include <format>
#include <iostream>
#include <filesystem>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Exception.h"

namespace fs = std::filesystem;

namespace libcbct {

namespace detail {

inline constexpr std::string_view kLogInfoColor = "\033[34m";
inline constexpr std::string_view kLogDebugColor = "\033[32m";
inline constexpr std::string_view kLogWarningColor = "\033[33m";
inline constexpr std::string_view kLogResetColor = "\033[0m";

// The implicit conversion captures the caller before entering a variadic logging
// function. A trailing default argument after a parameter pack cannot do this
// while preserving calls such as LIBCBCT_INFO("value={}", value).
template <typename... Args>
struct FormatWithLocation {
    std::format_string<Args...> format;
    std::source_location location;

    constexpr FormatWithLocation(std::format_string<Args...> fmt,
                                 std::source_location where = std::source_location::current())
        : format(fmt)
        , location(where) {
    }

    template <typename T>
    consteval FormatWithLocation(const T &fmt, std::source_location where = std::source_location::current())
        : format(fmt)
        , location(where) {
    }
};

inline std::string diagnosticMessage(std::string_view message, std::source_location where, bool shortVersion = false) {
    fs::path file_path(where.file_name());
    const std::string base_name = file_path.filename().string();
    if (shortVersion) {
        return std::format("{} ({}:{})", message, base_name, where.line());
    }
    return std::format("{} ({}:{} in {})", message, base_name, where.line(), where.function_name());
}

inline void writeLog(std::string_view color, std::string_view level, std::string_view message,
                     std::source_location where, bool shortVersion = false) {
    std::cout << color << '[' << level << "] " << kLogResetColor << diagnosticMessage(message, where, shortVersion)
              << std::endl;
}

}  // namespace detail

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
void LIBCBCT_INFO(detail::FormatWithLocation<std::type_identity_t<Args>...> fmt, Args &&...args) {
    detail::writeLog(detail::kLogInfoColor, "INFO", std::format(fmt.format, std::forward<Args>(args)...), fmt.location,
                     true);
}

template <typename... Args>
void LIBCBCT_DEBUG(detail::FormatWithLocation<std::type_identity_t<Args>...> fmt, Args &&...args) {
    detail::writeLog(detail::kLogDebugColor, "DEBUG", std::format(fmt.format, std::forward<Args>(args)...),
                     fmt.location, true);
}

template <typename... Args>
void LIBCBCT_WARN(detail::FormatWithLocation<std::type_identity_t<Args>...> fmt, Args &&...args) {
    detail::writeLog(detail::kLogWarningColor, "WARNING", std::format(fmt.format, std::forward<Args>(args)...),
                     fmt.location, true);
}

template <typename E = libcbct::Error, typename... Args>
[[noreturn]] void LIBCBCT_THROW(detail::FormatWithLocation<std::type_identity_t<Args>...> fmt, Args &&...args) {
    throw E(detail::diagnosticMessage(std::format(fmt.format, std::forward<Args>(args)...), fmt.location));
}

template <typename... Args>
[[noreturn]] void LIBCBCT_ERROR(detail::FormatWithLocation<std::type_identity_t<Args>...> fmt, Args &&...args) {
    LIBCBCT_THROW<libcbct::Error>(fmt, std::forward<Args>(args)...);
}

// -----------------------------------------------------------------------------
// Assertion with message
// -----------------------------------------------------------------------------

inline std::string ASRT_LABEL(std::string_view message, std::source_location where = std::source_location::current()) {
    return detail::diagnosticMessage(std::format("Assertion failed: {}", message), where);
}

// These are ordinary functions: arguments are evaluated even when assertions
// are disabled. Keep side effects out of assertion arguments.
template <typename... Args>
void LIBCBCT_ASSERT([[maybe_unused]] bool predicate,
                    [[maybe_unused]] detail::FormatWithLocation<std::type_identity_t<Args>...> fmt,
                    [[maybe_unused]] Args &&...args) {
#if !defined(LIBCBCT_DISABLE_ASSERT)
    if (!predicate) {
        std::cerr << ASRT_LABEL(std::format(fmt.format, std::forward<Args>(args)...), fmt.location) << std::endl;
        std::abort();
    }
#endif
}

inline void LIBCBCT_ASSERT([[maybe_unused]] bool predicate,
                           [[maybe_unused]] std::source_location where = std::source_location::current()) {
#if !defined(LIBCBCT_DISABLE_ASSERT)
    if (!predicate) {
        std::cerr << ASRT_LABEL("Condition is false", where) << std::endl;
        std::abort();
    }
#endif
}

}  // namespace libcbct

#endif  // LIBCBCT_LOGGING_H
