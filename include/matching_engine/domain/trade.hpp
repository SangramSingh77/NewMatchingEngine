#pragma once
#include "../common/types.hpp"

namespace me::domain {

struct Trade {
    OrderId_t buy_id;
    OrderId_t sell_id;
    Qty_t qty;
    Price_t price;
};

} // namespace me::domain
