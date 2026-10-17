#include "Common/Logging.h"

int main() {
    int calls = 0;
    libcbct::LIBCBCT_ASSERT(++calls == 0);
    libcbct::LIBCBCT_ASSERT(false, "Argument was evaluated: {}", ++calls);
    return calls == 2 ? 0 : 1;
}
