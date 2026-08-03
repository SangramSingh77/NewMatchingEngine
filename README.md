# NewMatchingEngine

A single-process, in-memory L3 limit-order matching engine written in C++20.
It accepts orders from standard input and emits trade and order-status events
to standard output.

## Build and run

The executable has no runtime dependencies beyond a C++20 compiler. CMake
downloads GoogleTest only when `BUILD_TESTS=ON`.

```sh
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/matching_engine < tests/data/basic_flow.in
```

`matching_engine` reads these CSV messages:

```text
0,orderid,side,quantity,price  # side: 0 = buy, 1 = sell
1,orderid                      # cancel a resting order
```

Blank lines and lines starting with `//` are ignored. Protocol events are
written to stdout and invalid input is reported to stderr.

See [docs/architecture.md](docs/architecture.md) for design and complexity
notes, and [tests/data](tests/data) for example input/output datasets.
