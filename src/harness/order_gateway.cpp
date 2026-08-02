#include "order_gateway.hpp"
#include <iostream>

namespace me::harness {

bool parse_input_line(const std::string& line) {
    // very small parser placeholder
    if (line.empty()) return false;
    if (line.rfind("//", 0) == 0) return false;
    // accept and ignore in this stub
    std::cout << "[gateway] received: " << line << std::endl;
    return true;
}

} // namespace me::harness
