#pragma once
#include "../book/limit_order_book.hpp"
#include <atomic>

namespace me::engine {

class MatchingTicker {
public:
    MatchingTicker() = default;
    ~MatchingTicker() = default;

    void start() { running_.store(true); }
    void stop() { running_.store(false); }

    bool running() const { return running_.load(); }

    me::book::LimitOrderBook& book() { return book_; }

private:
    std::atomic<bool> running_{false};
    me::book::LimitOrderBook book_;
};

} // namespace me::engine
