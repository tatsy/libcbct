#define _USE_MATH_DEFINES
#include <math.h>

#include "Api.h"

namespace libcbct {

struct PiType {
    __forceinline operator double() const {
        return double(M_PI);
    }
    __forceinline operator float() const {
        return float(M_PI);
    }
};

const constexpr PiType kPi = PiType();

struct HalfPiType {
    __forceinline operator double() const {
        return double(M_PI_2);
    }
    __forceinline operator float() const {
        return float(M_PI_2);
    }
};

const constexpr HalfPiType kHalfPi = HalfPiType();

struct TwoPiType {
    __forceinline operator double() const {
        return double(2.0 * M_PI);
    }
    __forceinline operator float() const {
        return float(2.0f * M_PI);
    }
};

const constexpr TwoPiType kTwoPi = TwoPiType();

struct FourPiType {
    __forceinline operator double() const {
        return double(4.0 * M_PI);
    }
    __forceinline operator float() const {
        return float(4.0f * M_PI);
    }
};

const constexpr FourPiType kFourPi = FourPiType();

struct OneOverPiType {
    __forceinline operator double() const {
        return double(M_1_PI);
    }
    __forceinline operator float() const {
        return float(M_1_PI);
    }
};

const constexpr OneOverPiType kOneOverPi = OneOverPiType();

struct OneOverTwoPiType {
    __forceinline operator double() const {
        return double(0.5 * M_1_PI);
    }
    __forceinline operator float() const {
        return float(0.5f * M_1_PI);
    }
};

const constexpr OneOverTwoPiType kOneOverTwoPi = OneOverTwoPiType();

}  // namespace libcbct
