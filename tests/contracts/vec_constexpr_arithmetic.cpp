#include "Utils/Vec.h"

namespace libcbct::tests {
template <int N>
constexpr bool arithmetic() {
    constexpr auto a = [] {
        if constexpr (N == 2) return vec2i(2, 4);
        else if constexpr (N == 3) return vec3i(2, 4, 6);
        else return vec4i(2, 4, 6, 8);
    }();
    constexpr auto sum = a + a;
    constexpr auto difference = a - a;
    constexpr auto product = a * a;
    constexpr auto quotient = a / a;
    constexpr auto negative = -a;
    for (int i = 0; i < N; ++i) {
        const int value = 2 * (i + 1);
        if (sum[i] != 2 * value || difference[i] != 0 ||
            product[i] != value * value || quotient[i] != 1 || negative[i] != -value) return false;
    }
    return (a == a) && !(a != a) && (a != sum) && !(a == sum);
}
static_assert(arithmetic<2>());
static_assert(arithmetic<3>());
static_assert(arithmetic<4>());
}  // namespace libcbct::tests

int main() { return 0; }
