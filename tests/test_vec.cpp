#include "Utils/Vec.h"

namespace libcbct::tests {

constexpr vec3i v(1, 2, 3);
static_assert(v[0] == 1);
static_assert(v[2] == 3);

constexpr int changed = [] {
    vec3i value(1, 2, 3);
    value[1] = 10;
    return value[1];
}();

static_assert(changed == 10);

}  // namespace libcbct::tests
