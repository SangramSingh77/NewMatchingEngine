#include "matching_engine.hpp"
#include <algorithm>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

namespace nme {

struct MatchingEngine::Impl {
    EventCallbacks cb;
    // simple incremental timestamp to maintain FIFO among same-price orders
    uint64_t seq = 0;

    struct Order {
        int id;
        Side side;
        int qty;
        double price;
        uint64_t ts; // sequence
    };

    // Price -> deque of Orders (resting orders). For buys, highest price first in map ordering.
    // We'll keep two maps with different comparators.
    using BuyPriceMap = map<double, deque<Order>, greater<double>>;
    using SellPriceMap = map<double, deque<Order>, less<double>>;

    BuyPriceMap buys;
    SellPriceMap sells;

    // Index order id -> location: side, price, index in deque isn't stable so store ts and iterate when removing.
    struct OrderIndex { Side side; double price; uint64_t ts; };
    unordered_map<int, OrderIndex> order_index;

    Impl(EventCallbacks c) : cb(move(c)) {}

    void log_error(const string &s) {
        if (cb.on_error) cb.on_error(s);
        else cerr << s << '\n';
    }

    void emit_trade(int qty, double price) {
        if (cb.on_trade) cb.on_trade(qty, price);
        else cout << "2," << qty << "," << price << '\n';
    }
    void emit_filled(int orderid) {
        if (cb.on_filled) cb.on_filled(orderid);
        else cout << "3," << orderid << '\n';
    }
    void emit_partial(int orderid, int remaining) {
        if (cb.on_partially_filled) cb.on_partially_filled(orderid, remaining);
        else cout << "4," << orderid << "," << remaining << '\n';
    }

    optional<Order> pop_resting_best(Side aggressive_side) {
        // Not used directly
        return {};
    }

    void add_resting_order(const Order &o) {
        if (o.side == Side::Buy) {
            buys[o.price].push_back(o);
        } else {
            sells[o.price].push_back(o);
        }
        order_index[o.id] = {o.side, o.price, o.ts};
    }

    // Remove resting order by id. Returns true if removed.
    bool remove_resting_by_id(int orderid) {
        auto it = order_index.find(orderid);
        if (it == order_index.end()) return false;
        Side side = it->second.side;
        double price = it->second.price;
        uint64_t ts = it->second.ts;
        if (side == Side::Buy) {
            auto mit = buys.find(price);
            if (mit == buys.end()) return false;
            auto &dq = mit->second;
            auto dit = std::find_if(dq.begin(), dq.end(), [&](const Order &o){ return o.id == orderid && o.ts == ts; });
            if (dit == dq.end()) return false;
            dq.erase(dit);
            if (dq.empty()) buys.erase(mit);
        } else {
            auto mit = sells.find(price);
            if (mit == sells.end()) return false;
            auto &dq = mit->second;
            auto dit = std::find_if(dq.begin(), dq.end(), [&](const Order &o){ return o.id == orderid && o.ts == ts; });
            if (dit == dq.end()) return false;
            dq.erase(dit);
            if (dq.empty()) sells.erase(mit);
        }
        order_index.erase(it);
        return true;
    }

    void match_aggressive(Order aggressive) {
        // aggressive is the incoming order; match against opposite book
        if (aggressive.qty <= 0) return;
        if (aggressive.side == Side::Buy) {
            // match against sells: lowest price first; crossing condition: buy.price >= sell.price
            while (aggressive.qty > 0 && !sells.empty()) {
                auto sit = sells.begin();
                double rest_price = sit->first;
                if (aggressive.price < rest_price) break; // no crossing
                auto &dq = sit->second;
                // get oldest resting order
                auto resting = dq.front();
                int exec_qty = min(aggressive.qty, resting.qty);
                // Execute at resting.price
                emit_trade(exec_qty, resting.price);
                // Update quantities
                aggressive.qty -= exec_qty;
                auto resting_id = resting.id;
                resting.qty -= exec_qty;
                // Aggressive status
                if (aggressive.qty == 0) emit_filled(aggressive.id);
                else emit_partial(aggressive.id, aggressive.qty);
                // Resting status
                if (resting.qty == 0) {
                    emit_filled(resting_id);
                    // remove resting from deque and index
                    dq.pop_front();
                    order_index.erase(resting_id);
                    if (dq.empty()) sells.erase(sit);
                } else {
                    // update front order qty
                    dq.front().qty = resting.qty;
                    // update index not needed (qty not stored there)
                    emit_partial(resting_id, resting.qty);
                }
            }
        } else {
            // aggressive is Sell: match against buys (highest first). crossing if buy.price >= sell.price
            while (aggressive.qty > 0 && !buys.empty()) {
                auto bit = buys.begin();
                double rest_price = bit->first;
                if (rest_price < aggressive.price) break;
                auto &dq = bit->second;
                auto resting = dq.front();
                int exec_qty = min(aggressive.qty, resting.qty);
                emit_trade(exec_qty, resting.price);
                aggressive.qty -= exec_qty;
                int resting_id = resting.id;
                resting.qty -= exec_qty;
                // Aggressive status
                if (aggressive.qty == 0) emit_filled(aggressive.id);
                else emit_partial(aggressive.id, aggressive.qty);
                // Resting status
                if (resting.qty == 0) {
                    emit_filled(resting_id);
                    dq.pop_front();
                    order_index.erase(resting_id);
                    if (dq.empty()) buys.erase(bit);
                } else {
                    dq.front().qty = resting.qty;
                    emit_partial(resting_id, resting.qty);
                }
            }
        }

        // if remaining qty > 0, insert as new resting order
        if (aggressive.qty > 0) {
            // we must ensure order id is unique
            if (order_index.find(aggressive.id) != order_index.end()) {
                log_error("Duplicate order ID: " + to_string(aggressive.id));
                return;
            }
            aggressive.ts = ++seq;
            add_resting_order(aggressive);
        }
    }
};

MatchingEngine::MatchingEngine(EventCallbacks cb) : p(new Impl(cb)) {}

void MatchingEngine::process_message_line(const string &line) {
    // ignore comments starting with // and empty lines
    string s = line;
    // trim
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == string::npos) return; // blank
    if (s.substr(first,2) == "//") return;
    // parse CSV tokens
    vector<string> toks;
    string tok;
    stringstream ss(s);
    while (getline(ss, tok, ',')) toks.push_back(tok);
    if (toks.empty()) return;
    // trim tokens
    for (auto &t : toks) {
        size_t a = t.find_first_not_of(" \t\r\n");
        size_t b = t.find_last_not_of(" \t\r\n");
        if (a == string::npos) t = ""; else t = t.substr(a, b - a + 1);
    }
    try {
        int msgtype = stoi(toks[0]);
        if (msgtype == 0) {
            if (toks.size() != 5) { p->log_error("Malformed AddOrderRequest: " + line); return; }
            int orderid = stoi(toks[1]);
            int side = stoi(toks[2]);
            int qty = stoi(toks[3]);
            double price = stod(toks[4]);
            if (qty <= 0 || orderid <= 0) { p->log_error("Invalid order values: " + line); return; }
            add_order(orderid, side==0?Side::Buy:Side::Sell, qty, price);
        } else if (msgtype == 1) {
            if (toks.size() != 2) { p->log_error("Malformed CancelOrderRequest: " + line); return; }
            int orderid = stoi(toks[1]);
            cancel_order(orderid);
        } else {
            p->log_error("Unknown message type: " + toks[0]);
        }
    } catch (const exception &e) {
        p->log_error(string("Malformed input: ") + e.what() + " -- " + line);
    }
}

void MatchingEngine::add_order(int orderid, Side side, int qty, double price) {
    // check duplicates
    if (p->order_index.find(orderid) != p->order_index.end()) {
        p->log_error("Duplicate order ID: " + to_string(orderid));
        return;
    }
    p->seq++;
    Impl::Order o{orderid, side, qty, price, p->seq};
    // aggressive order: attempt to match
    p->match_aggressive(o);
}

void MatchingEngine::cancel_order(int orderid) {
    bool removed = p->remove_resting_by_id(orderid);
    if (!removed) {
        p->log_error("Cancel failed, unknown order id: " + to_string(orderid));
    }
}

} // namespace nme
