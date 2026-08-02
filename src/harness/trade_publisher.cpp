#include "trade_publisher.hpp"
#include <iostream>

namespace me::harness {

void publish_trade(const me::domain::Trade& t) {
    std::cout << "2," << t.qty << "," << t.price << std::endl;
}

} // namespace me::harness
