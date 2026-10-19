#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_CHECKED_SIZE_H
#define LIBCBCT_CHECKED_SIZE_H

#include <concepts>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "Common/Exception.h"

namespace libcbct::detail {

// Accept integer types, but not bool
template <typename T>
concept SizeInteger = std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>;

template <SizeInteger T>
constexpr std::size_t checkedSize(T value) {
    if constexpr (std::signed_integral<T>) {
        if (value < 0) {
            throw libcbct::InvalidArgument("Size must be nonnegative");
        }
    }

    if constexpr (std::numeric_limits<T>::digits > std::numeric_limits<std::size_t>::digits) {
        constexpr auto maxSize = std::numeric_limits<std::size_t>::max();
        if (value > static_cast<T>(maxSize)) {
            throw libcbct::InvalidArgument("Size exceeds size_t range");
        }
    }

    return static_cast<std::size_t>(value);
}

constexpr std::size_t checkedProductImpl(std::size_t value) noexcept {
    return value;
}

template <std::same_as<std::size_t>... Rest>
constexpr std::size_t checkedProductImpl(std::size_t a, std::size_t b, Rest... rest) {
    if (b != 0 && a > std::numeric_limits<std::size_t>::max() / b) {
        throw libcbct::InvalidArgument("Size product overflow");
    }

    return checkedProductImpl(a * b, rest...);
}

template <SizeInteger... Args>
    requires(sizeof...(Args) > 0)
constexpr std::size_t checkedProduct(Args... args) {
    return checkedProductImpl(checkedSize(args)...);
}

}  // namespace libcbct::detail

#endif  // LIBCBCT_CHECKED_SIZE_H
