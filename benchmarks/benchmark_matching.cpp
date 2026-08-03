#include "matching_engine.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>

int main() {
    constexpr std::uint64_t order_count = 50'000;
    nme::EventCallbacks callbacks;
    callbacks.on_trade = [](nme::Quantity, nme::Price) {};
    callbacks.on_fully_filled = [](nme::OrderId) {};
    callbacks.on_partially_filled = [](nme::OrderId, nme::Quantity) {};
    callbacks.on_error = [](const std::string&) {};
    nme::MatchingEngine engine(callbacks);

    const auto started = std::chrono::steady_clock::now();
    for (std::uint64_t index = 0; index < order_count; ++index) {
        engine.OnAddOrder(index + 1, nme::Side::Sell, 1, 100.0);
        engine.OnAddOrder(order_count + index + 1, nme::Side::Buy, 1, 100.0);
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    const double seconds = std::chrono::duration<double>(elapsed).count();
    std::cout << "orders=" << order_count * 2 << ", seconds=" << seconds
              << ", throughput=" << static_cast<double>(order_count * 2) / seconds << " orders/sec\n";
}
