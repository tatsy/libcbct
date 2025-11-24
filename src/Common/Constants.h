#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_CONSTANTS_H
#define LIBCBCT_CONSTANTS_H

#define _USE_MATH_DEFINES
#include <cmath>

#include "Api.h"

namespace libcbct {

struct PiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return double(M_PI);
    }

    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return float(M_PI);
    }
};

constexpr PiType kPi = PiType();

struct HalfPiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return double(M_PI_2);
    }
    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return float(M_PI_2);
    }
};

constexpr HalfPiType kHalfPi = HalfPiType();

struct TwoPiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return double(2.0 * M_PI);
    }
    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return float(2.0f * M_PI);
    }
};

constexpr TwoPiType kTwoPi = TwoPiType();

struct FourPiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return double(4.0 * M_PI);
    }
    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return float(4.0f * M_PI);
    }
};

constexpr FourPiType kFourPi = FourPiType();

struct OneOverPiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return double(M_1_PI);
    }
    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return float(M_1_PI);
    }
};

constexpr OneOverPiType kOneOverPi = OneOverPiType();

struct OneOverTwoPiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return double(0.5 * M_1_PI);
    }
    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return float(0.5f * M_1_PI);
    }
};

constexpr OneOverTwoPiType kOneOverTwoPi = OneOverTwoPiType();

struct OneOverFourPiType {
    LIBCBCT_HOST_DEVICE constexpr operator double() const {
        return double(0.25 * M_1_PI);
    }
    LIBCBCT_HOST_DEVICE constexpr operator float() const {
        return float(0.25f * M_1_PI);
    }
};

constexpr OneOverFourPiType kOneOverFourPi = OneOverFourPiType();

}  // namespace libcbct

#endif  // LIBCBCT_CONSTANTS_H