#pragma once
#include "../domain/trade.hpp"

namespace me::harness {

void publish_trade(const me::domain::Trade& t);

} // namespace me::harness
