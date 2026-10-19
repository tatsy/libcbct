#include "Utils/Vec.h"

namespace libcbct::tests {
template <int N>
constexpr bool constructorsAndIndexing() {
    constexpr vec_t<int, N> zero;
    constexpr vec_t<int, N> fill(7);
    constexpr vec_t<double, N> converted(fill);
    auto changed = fill;
    for (int i = 0; i < N; ++i) {
        if (zero[i] != 0 || fill[i] != 7 || converted[i] != 7.0) return false;
        changed[i] = i + 1;
    }
    vec_t<int, N> assigned;
    assigned = changed;
    for (int i = 0; i < N; ++i) if (assigned[i] != i + 1) return false;
    return true;
}
static_assert(constructorsAndIndexing<2>());
static_assert(constructorsAndIndexing<3>());
static_assert(constructorsAndIndexing<4>());
static_assert(vec2i(1, 2)[1] == 2);
static_assert(vec3i(1, 2, 3)[2] == 3);
static_assert(vec4i(1, 2, 3, 4)[3] == 4);
}  // namespace libcbct::tests

int main() { return 0; }
