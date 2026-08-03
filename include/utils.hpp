#pragma once

#include "object_pools.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace nme::utils {

inline std::string_view Trim(std::string_view value) {
    constexpr std::string_view whitespace{" \t\r\n"};
    const auto first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    return value.substr(first, value.find_last_not_of(whitespace) - first + 1);
}

inline bool ParseUnsigned(std::string_view value, std::uint64_t& result) {
    if (value.empty()) return false;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    return error == std::errc{} && end == value.data() + value.size() && result != 0;
}

inline bool ParsePrice(std::string_view value, Price& result) {
    if (value.empty()) return false;
    const std::string text{value};
    char* end = nullptr;
    result = std::strtod(text.c_str(), &end);
    return end == text.c_str() + text.size() && std::isfinite(result) && result > 0.0;
}

inline std::vector<std::string_view> SplitCsv(std::string_view line) {
    std::vector<std::string_view> fields;
    while (true) {
        const auto comma = line.find(',');
        fields.push_back(Trim(line.substr(0, comma)));
        if (comma == std::string_view::npos) return fields;
        line.remove_prefix(comma + 1);
    }
}

} // namespace nme::utils
