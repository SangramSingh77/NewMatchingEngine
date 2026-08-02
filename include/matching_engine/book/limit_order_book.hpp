#pragma once
#include "order_book_side.hpp"

namespace me::book {

class LimitOrderBook {
public:
    LimitOrderBook() = default;

    void add_order(const me::domain::Order& o) {
        if (o.side == me::Side::Buy) bids_.add_order(o);
        else asks_.add_order(o);
    }

    bool cancel_order(me::OrderId_t id) {
        return bids_.cancel_order(id) || asks_.cancel_order(id);
    }

private:
    OrderBookSide bids_;
    OrderBookSide asks_;
};

} // namespace me::book
