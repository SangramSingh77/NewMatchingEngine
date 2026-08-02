#pragma once
#include "../common/types.hpp"
#include "../domain/order.hpp"
#include <deque>
#include <optional>

namespace me::book {

class OrderBookSide {
public:
    OrderBookSide() = default;

    void add_order(const me::domain::Order& o) {
        levels_[o.price].push_back(o);
    }

    bool cancel_order(me::OrderId_t id) {
        // naive scan - placeholder
        for (auto &kv : levels_) {
            auto &dq = kv.second;
            for (auto it = dq.begin(); it != dq.end(); ++it) {
                if (it->id == id) {
                    dq.erase(it);
                    return true;
                }
            }
        }
        return false;
    }

private:
    std::map<me::Price_t, std::deque<me::domain::Order>> levels_;
};

} // namespace me::book
