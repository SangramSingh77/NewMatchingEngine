# NewMatchingEngine - Build & Run

This repository contains a small matching engine prototype and a CMake project to build it.

Build (out-of-source, using Ninja or default generator):

  mkdir -p build
  cmake -S . -B build -DBUILD_TESTS=ON
  cmake --build build --parallel

Run the binary (reads stdin):

  echo "0,1,1,100,10" | ./build/bin/matching_engine_bin

Run tests:

  ctest --test-dir build --parallel

Notes:
- Core code uses only the C++ standard library. Tests use Catch2 via CMake FetchContent.
- The input format follows requirement.txt: Add orders: 0,orderid,side,quantity,price; Cancel: 1,orderid
