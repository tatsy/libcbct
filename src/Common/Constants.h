#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_CONSTANTS_H
#define LIBCBCT_CONSTANTS_H

#include <numbers>

#include "Api.h"

namespace libcbct {

struct PiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return std::numbers::pi_v<double>;
    }

    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return static_cast<float>(std::numbers::pi_v<double>);
    }
};

inline constexpr PiType kPi{};

struct HalfPiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return 0.5 * std::numbers::pi_v<double>;
    }
    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return static_cast<float>(0.5 * std::numbers::pi_v<double>);
    }
};

inline constexpr HalfPiType kHalfPi{};

struct TwoPiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return 2.0 * std::numbers::pi_v<double>;
    }
    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return static_cast<float>(2.0 * std::numbers::pi_v<double>);
    }
};

inline constexpr TwoPiType kTwoPi{};

struct FourPiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return 4.0 * std::numbers::pi_v<double>;
    }
    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return static_cast<float>(4.0 * std::numbers::pi_v<double>);
    }
};

inline constexpr FourPiType kFourPi{};

struct OneOverPiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return std::numbers::inv_pi_v<double>;
    }
    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return static_cast<float>(std::numbers::inv_pi_v<double>);
    }
};

inline constexpr OneOverPiType kOneOverPi{};

struct OneOverTwoPiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return 0.5 * std::numbers::inv_pi_v<double>;
    }
    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return static_cast<float>(0.5 * std::numbers::inv_pi_v<double>);
    }
};

inline constexpr OneOverTwoPiType kOneOverTwoPi{};

struct OneOverFourPiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return 0.25 * std::numbers::inv_pi_v<double>;
    }
    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return static_cast<float>(0.25 * std::numbers::inv_pi_v<double>);
    }
};

inline constexpr OneOverFourPiType kOneOverFourPi{};

}  // namespace libcbct

#endif  // LIBCBCT_CONSTANTS_H
