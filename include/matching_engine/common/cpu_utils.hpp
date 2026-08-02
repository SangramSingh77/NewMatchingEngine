#pragma once

#include <cstddef>

#define CACHELINE_ALIGN alignas(64)

namespace me::cpu {

inline void nop() {}

} // namespace me::cpu
