#pragma once
#include "../common/types.hpp"
#include <cstdint>

namespace me::domain {

struct Order {
    OrderId_t id;
    me::Side side;
    Qty_t qty;
    Price_t price;
    uint64_t ts;
};

} // namespace me::domain
