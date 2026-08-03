#include "matching_engine.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

struct EventRecorder {
    nme::EventCallbacks GetCallbacks() {
        return {
            [this](nme::Quantity quantity, nme::Price price) {
                m_events.emplace_back("2," + std::to_string(quantity) + "," + std::to_string(static_cast<int>(price)));
            },
            [this](nme::OrderId orderId) {
                m_events.emplace_back("3," + std::to_string(orderId));
            },
            [this](nme::OrderId orderId, nme::Quantity quantity) {
                m_events.emplace_back("4," + std::to_string(orderId) + "," + std::to_string(quantity));
            },
            [this](const std::string& error) {
                m_events.emplace_back("E:" + error);
            },
        };
    }

    std::vector<std::string> m_events;
};

void ExpectEvents(const EventRecorder& recorder, const std::vector<std::string>& expected) {
    EXPECT_EQ(recorder.m_events, expected);
}

TEST(MatchingEngineTest, ExecutesAtRestingOrderPrice) {
    EventRecorder recorder;
    nme::MatchingEngine engine(recorder.GetCallbacks());

    engine.OnAddOrder(1, nme::Side::Sell, 100, 10.0);
    engine.OnAddOrder(2, nme::Side::Buy, 100, 12.0);

    ExpectEvents(recorder, {"2,100,10", "3,2", "3,1"});
}

TEST(MatchingEngineTest, MaintainsFifoForOrdersAtSamePrice) {
    EventRecorder recorder;
    nme::MatchingEngine engine(recorder.GetCallbacks());

    engine.OnAddOrder(100, nme::Side::Sell, 100, 10.0);
    engine.OnAddOrder(101, nme::Side::Sell, 50, 10.0);
    engine.OnAddOrder(102, nme::Side::Buy, 120, 11.0);

    ExpectEvents(recorder, {
        "2,100,10", "4,102,20", "3,100",
        "2,20,10", "3,102", "4,101,30",
    });
}

TEST(MatchingEngineTest, UsesBestPriceBeforeWorsePrice) {
    EventRecorder recorder;
    nme::MatchingEngine engine(recorder.GetCallbacks());

    engine.OnAddOrder(10, nme::Side::Sell, 10, 10.0);
    engine.OnAddOrder(11, nme::Side::Sell, 10, 11.0);
    engine.OnAddOrder(12, nme::Side::Buy, 25, 11.0);

    ExpectEvents(recorder, {
        "2,10,10", "4,12,15", "3,10",
        "2,10,11", "4,12,5", "3,11",
    });
}

TEST(MatchingEngineTest, RestsNonCrossingOrderAndMatchesItLater) {
    EventRecorder recorder;
    nme::MatchingEngine engine(recorder.GetCallbacks());

    engine.OnAddOrder(20, nme::Side::Buy, 10, 10.0);
    engine.OnAddOrder(21, nme::Side::Sell, 10, 11.0);
    engine.OnAddOrder(22, nme::Side::Sell, 10, 10.0);

    ExpectEvents(recorder, {"2,10,10", "3,22", "3,20"});
}

TEST(MatchingEngineTest, CancelsMiddleOrderWithoutBreakingFifo) {
    EventRecorder recorder;
    nme::MatchingEngine engine(recorder.GetCallbacks());

    engine.OnAddOrder(30, nme::Side::Sell, 5, 10.0);
    engine.OnAddOrder(31, nme::Side::Sell, 5, 10.0);
    engine.OnAddOrder(32, nme::Side::Sell, 5, 10.0);
    engine.OnCancelOrder(31);
    engine.OnAddOrder(33, nme::Side::Buy, 10, 10.0);

    ExpectEvents(recorder, {
        "2,5,10", "4,33,5", "3,30",
        "2,5,10", "3,33", "3,32",
    });
}

TEST(MatchingEngineTest, CancelsRemainingQuantityAfterPartialFill) {
    EventRecorder recorder;
    nme::MatchingEngine engine(recorder.GetCallbacks());

    engine.OnAddOrder(40, nme::Side::Sell, 10, 10.0);
    engine.OnAddOrder(41, nme::Side::Buy, 4, 10.0);
    engine.OnCancelOrder(40);
    engine.OnAddOrder(42, nme::Side::Buy, 6, 10.0);

    ExpectEvents(recorder, {"2,4,10", "3,41", "4,40,6"});
}

TEST(MatchingEngineTest, RejectsDuplicateAndInvalidOrders) {
    EventRecorder recorder;
    nme::MatchingEngine engine(recorder.GetCallbacks());

    engine.OnAddOrder(50, nme::Side::Buy, 10, 10.0);
    engine.OnAddOrder(50, nme::Side::Sell, 10, 10.0);
    engine.OnAddOrder(0, nme::Side::Buy, 10, 10.0);
    engine.OnAddOrder(51, nme::Side::Buy, 0, 10.0);
    engine.OnAddOrder(52, nme::Side::Buy, 10, 0.0);

    ExpectEvents(recorder, {
        "E:Duplicate order ID: 50",
        "E:Invalid AddOrderRequest for order ID: 0",
        "E:Invalid AddOrderRequest for order ID: 51",
        "E:Invalid AddOrderRequest for order ID: 52",
    });
}

TEST(MatchingEngineTest, ReportsUnknownCancel) {
    EventRecorder recorder;
    nme::MatchingEngine engine(recorder.GetCallbacks());

    engine.OnCancelOrder(99);

    ExpectEvents(recorder, {"E:Cancel failed, unknown order ID: 99"});
}

TEST(MatchingEngineTest, ParserAcceptsWhitespaceCommentsAndBlankLines) {
    EventRecorder recorder;
    nme::MatchingEngine engine(recorder.GetCallbacks());

    engine.OnInput(" ");
    engine.OnInput("  // ignored comment");
    engine.OnInput(" 0 , 60 , 1 , 10 , 10.5 ");
    engine.OnInput("0,61,0,10,11");

    ExpectEvents(recorder, {"2,10,10", "3,61", "3,60"});
}

TEST(MatchingEngineTest, ParserReportsMalformedAndUnknownMessages) {
    EventRecorder recorder;
    nme::MatchingEngine engine(recorder.GetCallbacks());

    engine.OnInput("0,1,2,10,10");
    engine.OnInput("0,1,0,abc,10");
    engine.OnInput("1,not-an-id");
    engine.OnInput("9,1");

    ExpectEvents(recorder, {
        "E:Malformed AddOrderRequest: 0,1,2,10,10",
        "E:Malformed AddOrderRequest: 0,1,0,abc,10",
        "E:Malformed CancelOrderRequest: 1,not-an-id",
        "E:Unknown message type: 9",
    });
}

} // namespace
