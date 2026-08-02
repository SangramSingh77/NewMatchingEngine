#pragma once
#include <cstdint>

namespace me {

using Price_t = int64_t; // price in ticks
using Qty_t = int32_t;
using OrderId_t = int32_t;

enum class Side : uint8_t { Buy = 0, Sell = 1 };

} // namespace me
