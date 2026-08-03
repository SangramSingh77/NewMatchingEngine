#include "matching_engine.hpp"

#include <iostream>
#include <string>

int main() {
    nme::MatchingEngine engine;
    std::string line;

    while (std::getline(std::cin, line)) {
        engine.OnInput(line);
    }
    return 0;
}
