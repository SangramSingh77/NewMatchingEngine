#include "matching_engine.hpp"
#include <iostream>
#include <string>

int main() {
    nme::EventCallbacks cb;
    // default callbacks print to stdout/stderr via the library's defaults, but we wire errors to cerr explicitly
    cb.on_error = [](const std::string &e){ std::cerr << e << std::endl; };
    nme::MatchingEngine eng(cb);
    std::string line;
    while (std::getline(std::cin, line)) {
        eng.process_message_line(line);
    }
    return 0;
}
