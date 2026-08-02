#pragma once
#include <string>

namespace me::harness {

// Parse a single input line and forward to callbacks in higher layer.
bool parse_input_line(const std::string& line);

} // namespace me::harness
