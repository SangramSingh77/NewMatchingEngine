#pragma once

#include <functional>
#include <string>

namespace nme {

enum class Side { Buy = 0, Sell = 1 };

struct EventCallbacks {
    // trade qty and price
    std::function<void(int, double)> on_trade;
    // order fully filled
    std::function<void(int)> on_filled;
    // order partially filled with remaining qty
    std::function<void(int, int)> on_partially_filled;
    // error logging
    std::function<void(const std::string &)> on_error;
};

class MatchingEngine {
public:
    explicit MatchingEngine(EventCallbacks cb = {});

    // Process an input line like in requirement: either "0,orderid,side,quantity,price" or "1,orderid"
    void process_message_line(const std::string &line);

    // Lower-level API for tests
    void add_order(int orderid, Side side, int qty, double price);
    void cancel_order(int orderid);

private:
    struct Impl;
    Impl *p;
};

} // namespace nme
