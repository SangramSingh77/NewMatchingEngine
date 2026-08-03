#pragma once

#include "order_book.hpp"

#include <functional>
#include <string>

namespace nme {

struct EventCallbacks {
    std::function<void(Quantity, Price)> on_trade;
    std::function<void(OrderId)> on_fully_filled;
    std::function<void(OrderId, Quantity)> on_partially_filled;
    std::function<void(const std::string&)> on_error;
};

class MatchingEngine {
public:
    static constexpr std::size_t kMaxOrders = 65'536;
    static constexpr std::size_t kOrderIndexCapacity = 131'071;

    explicit MatchingEngine(EventCallbacks callbacks = {});

    void OnInput(const std::string& line);
    void OnAddOrder(OrderId order_id, Side side, Quantity quantity, Price price);
    void OnCancelOrder(OrderId order_id);

private:
    void Match(Order& incoming);
    void Execute(Order& incoming, OrderIndex resting_index);
    void Rest(const Order& incoming);
    void RemoveRestingOrder(OrderIndex index);
    void Error(const std::string& message) const;

    EventCallbacks m_callbacks;
    OrderPool<kMaxOrders> m_order_pool;
    OrderManager<kOrderIndexCapacity> m_order_manager;
    OrderBook<kMaxOrders> m_order_book;
};

} // namespace nme
