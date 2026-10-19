#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_PARSE_NUMBER_H
#define LIBCBCT_PARSE_NUMBER_H

#include <format>
#include <concepts>
#include <charconv>
#include <type_traits>

#include "Common/Exception.h"

namespace libcbct::detail {

template <typename T>
    requires(std::same_as<T, int> || std::same_as<T, float>)
T parseNumber(std::string_view text) {
    if (text.empty()) {
        throw libcbct::InvalidArgument(std::format("Invalid number text: {}", text));
    }

    T value{};
    const auto end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, value);
    if (ec != std::errc{} || ptr != end) {
        throw libcbct::InvalidArgument(std::format("Invalid number text: {}", text));
    }

    return value;
}

}  // namespace libcbct::detail

#endif  // LIBCBCT_PARSE_NUMBER_H
