# Architecture

## Matching flow

`MatchingEngine` validates stdin CSV messages and matches each add request immediately against the
opposite book. A buy consumes the lowest ask while its limit price crosses;
a sell consumes the highest bid while its limit price crosses. Each execution
uses the resting order's price and produces events in this order: trade,
aggressive status, resting status.

Unfilled incoming quantity is appended to its own price-level FIFO queue.
Cancel requests affect resting orders only.

## Book representation

Each side uses a sorted contiguous `PriceIndex` of `(price, price-level ID)`
pairs. `PriceLevel` values come from a fixed pool, while orders live in a
fixed `OrderPool` and each price level references them by index:

| Structure | Purpose |
| --- | --- |
| sorted buy price array | best bid is its last entry |
| sorted sell price array | best ask is its first entry |
| doubly linked arena indices | FIFO arrival order at a price |
| fixed open-addressing hash index | order ID to arena index |

The fixed order and price-level pools allocate and free slots through free
lists in `O(1)`; they perform no heap allocation while creating or cancelling
orders. The ID index makes cancellation and removal of a fully filled resting
order constant time after locating its price level. Best-price lookup is
constant time. A price lookup is `O(log P)` through binary search; inserting
or removing a price shifts the compact price array and costs `O(P)`.

## Complexity

- Best-price crossing check: `O(1)`.
- Add without a match: `O(log P)` to find/create the price level.
- Each matched resting order: `O(1)` apart from a price-level deletion, which
  is `O(log P)`.
- Cancel: `O(1)` list removal through the ID index, plus `O(log P)` only if it
  empties a price level and that level must be erased.

Here `P` is the number of price levels. A single aggressive order is naturally
linear in the number of resting orders it executes against.

## Current trade-offs and production direction

`double` prices are retained at the public interface to match the supplied
protocol, which is convenient but unsuitable for a production exchange due to
binary floating-point representation. A production version should parse prices
to validated integer ticks. It should also use a bounded arena/slab allocator,
preallocated ID tables or carefully sized hash maps, structured telemetry,
per-instrument shards, and a deterministic sequence/recovery log. Those are
outside this single-process V1 scope.
