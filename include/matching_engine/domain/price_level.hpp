#pragma once
#include "../common/types.hpp"

namespace me::domain {

struct PriceLevel {
    Price_t price;
    Qty_t total_qty{
        0
    };
    uint32_t order_count{0};
};

} // namespace me::domain
