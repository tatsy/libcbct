#pragma once

#if defined(LIBCBCT_WITH_CUDA)

#include "FeldkampCUDA.h"

namespace libcbct {

// Source-compatible name retained for callers of the former PTX-specific class.
using FeldkampSlangCUDA = FeldkampCUDA;

}  // namespace libcbct

#endif  // LIBCBCT_WITH_CUDA
