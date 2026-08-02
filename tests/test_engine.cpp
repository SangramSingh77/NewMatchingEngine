#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "matching_engine.hpp"
#include <vector>
#include <string>

using namespace nme;

TEST_CASE("Basic add and match") {
    std::vector<std::string> events;
    EventCallbacks cb;
    cb.on_trade = [&](int q, double p){ events.push_back("2," + std::to_string(q) + "," + std::to_string((int)p)); };
    cb.on_filled = [&](int id){ events.push_back("3," + std::to_string(id)); };
    cb.on_partially_filled = [&](int id, int rem){ events.push_back("4," + std::to_string(id) + "," + std::to_string(rem)); };
    cb.on_error = [&](const std::string &s){ events.push_back(std::string("E:") + s); };

    MatchingEngine eng(cb);
    // Add a sell resting order
    eng.add_order(1, Side::Sell, 100, 10.0);
    // Add a buy aggressive order that matches partially
    eng.add_order(2, Side::Buy, 40, 11.0);
    // Expect trade (40@10), aggressive partially filled (remaining 0? actually 40 filled so aggressive filled), resting partially filled (60 remaining)
    REQUIRE(events.size() == 3);
    REQUIRE(events[0] == "2,40,10");
    REQUIRE(events[1] == "3,2");
    REQUIRE(events[2] == "4,1,60");
}
