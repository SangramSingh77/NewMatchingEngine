#include "matching_engine.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace nme {
namespace {

std::string_view Trim(std::string_view value) {
    constexpr std::string_view whitespace{" \t\r\n"};
    const auto first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    return value.substr(first, value.find_last_not_of(whitespace) - first + 1);
}

bool ParseUnsigned(std::string_view value, std::uint64_t& result) {
    if (value.empty()) return false;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    return error == std::errc{} && end == value.data() + value.size() && result != 0;
}

bool ParsePrice(std::string_view value, Price& result) {
    std::string text{value};
    char* end = nullptr;
    result = std::strtod(text.c_str(), &end);
    return end == text.c_str() + text.size() && std::isfinite(result) && result > 0.0;
}

std::vector<std::string_view> Split(std::string_view line) {
    std::vector<std::string_view> fields;
    while (true) {
        const auto comma = line.find(',');
        fields.push_back(Trim(line.substr(0, comma)));
        if (comma == std::string_view::npos) return fields;
        line.remove_prefix(comma + 1);
    }
}

} // namespace

MatchingEngine::MatchingEngine(EventCallbacks callbacks) : m_callbacks(std::move(callbacks)) {}

void MatchingEngine::OnInput(const std::string& line) {
    const std::string_view input = Trim(line);
    if (input.empty() || input.rfind("//", 0) == 0) return;
    const auto fields = Split(input);

    if (fields[0] == "0") {
        OrderId id = 0;
        Quantity quantity = 0;
        Price price = 0.0;
        if (fields.size() != 5 || !ParseUnsigned(fields[1], id) || !ParseUnsigned(fields[3], quantity) ||
            !ParsePrice(fields[4], price) || (fields[2] != "0" && fields[2] != "1")) {
            Error("Malformed AddOrderRequest: " + line);
            return;
        }
        OnAddOrder(id, fields[2] == "0" ? Side::Buy : Side::Sell, quantity, price);
        return;
    }
    if (fields[0] == "1") {
        OrderId id = 0;
        if (fields.size() != 2 || !ParseUnsigned(fields[1], id)) Error("Malformed CancelOrderRequest: " + line);
        else OnCancelOrder(id);
        return;
    }
    Error("Unknown message type: " + std::string(fields[0]));
}

void MatchingEngine::OnAddOrder(OrderId order_id, Side side, Quantity quantity, Price price) {
    if (order_id == 0 || quantity == 0 || !std::isfinite(price) || price <= 0.0) {
        Error("Invalid AddOrderRequest for order ID: " + std::to_string(order_id));
        return;
    }
    if (m_order_manager.Get(order_id) != kInvalidOrderIndex) {
        Error("Duplicate order ID: " + std::to_string(order_id));
        return;
    }
    Order incoming{false, order_id, side, quantity, price};
    Match(incoming);
}

void MatchingEngine::OnCancelOrder(OrderId order_id) {
    const OrderIndex index = m_order_manager.Get(order_id);
    if (index == kInvalidOrderIndex) Error("Cancel failed, unknown order ID: " + std::to_string(order_id));
    else RemoveRestingOrder(index);
}

void MatchingEngine::Match(Order& incoming) {
    const Side opposite = incoming.side == Side::Buy ? Side::Sell : Side::Buy;
    while (incoming.quantity > 0) {
        const auto resting_index = m_order_book.BestOrder(opposite);
        if (!resting_index.has_value()) break;
        const Order& resting = m_order_pool.Get(*resting_index);
        const bool crosses = incoming.side == Side::Buy ? incoming.price >= resting.price : resting.price >= incoming.price;
        if (!crosses) break;
        Execute(incoming, *resting_index);
    }
    if (incoming.quantity > 0) Rest(incoming);
}

void MatchingEngine::Execute(Order& incoming, OrderIndex resting_index) {
    Order& resting = m_order_pool.Get(resting_index);
    const Quantity quantity = std::min(incoming.quantity, resting.quantity);
    if (m_callbacks.on_trade) m_callbacks.on_trade(quantity, resting.price);
    else std::cout << "2," << quantity << ',' << std::setprecision(15) << resting.price << '\n';

    incoming.quantity -= quantity;
    m_order_book.ReduceQuantity(resting_index, quantity, m_order_pool);
    resting.quantity -= quantity;
    if (incoming.quantity == 0) {
        if (m_callbacks.on_fully_filled) m_callbacks.on_fully_filled(incoming.id);
        else std::cout << "3," << incoming.id << '\n';
    } else {
        if (m_callbacks.on_partially_filled) m_callbacks.on_partially_filled(incoming.id, incoming.quantity);
        else std::cout << "4," << incoming.id << ',' << incoming.quantity << '\n';
    }
    if (resting.quantity == 0) {
        const OrderId id = resting.id;
        if (m_callbacks.on_fully_filled) m_callbacks.on_fully_filled(id);
        else std::cout << "3," << id << '\n';
        RemoveRestingOrder(resting_index);
    } else {
        if (m_callbacks.on_partially_filled) m_callbacks.on_partially_filled(resting.id, resting.quantity);
        else std::cout << "4," << resting.id << ',' << resting.quantity << '\n';
    }
}

void MatchingEngine::Rest(const Order& incoming) {
    const OrderIndex index = m_order_pool.Allocate(incoming);
    if (index == kInvalidOrderIndex) { Error("Order pool capacity exceeded"); return; }
    if (!m_order_manager.Add(incoming.id, index)) { m_order_pool.Free(index); Error("Order index capacity exceeded"); return; }
    if (!m_order_book.Add(index, m_order_pool)) {
        m_order_manager.Remove(incoming.id);
        m_order_pool.Free(index);
        Error("Price level pool capacity exceeded");
    }
}

void MatchingEngine::RemoveRestingOrder(OrderIndex index) {
    const OrderId id = m_order_pool.Get(index).id;
    m_order_book.Remove(index, m_order_pool);
    m_order_manager.Remove(id);
    m_order_pool.Free(index);
}

void MatchingEngine::Error(const std::string& message) const {
    if (m_callbacks.on_error) m_callbacks.on_error(message);
    else std::cerr << message << '\n';
}

} // namespace nme
