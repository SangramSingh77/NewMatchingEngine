#include "engine/matching_ticker.hpp"
#include "harness/order_gateway.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    me::engine::MatchingTicker ticker;
    ticker.start();

    std::string line;
    while (std::getline(std::cin, line)) {
        me::harness::parse_input_line(line);
        if (!ticker.running()) break;
    }

    ticker.stop();
    return 0;
}
