#pragma once

#include "object_pools.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace nme {

using PriceLevelId = std::uint32_t;
constexpr PriceLevelId kInvalidPriceLevelId = std::numeric_limits<PriceLevelId>::max();

/** One reusable price-level object. Its order indices form a FIFO/LRU chain. */
struct PriceLevel {
    bool active = false;
    Price price = 0.0;
    OrderIndex head = kInvalidOrderIndex;
    OrderIndex tail = kInvalidOrderIndex;
    Quantity total_quantity = 0;
    std::uint32_t order_count = 0;
};

template <std::size_t Capacity>
class PriceLevelPool {
public:
    static_assert(Capacity > 0, "PriceLevelPool requires at least one slot");
    static_assert(Capacity < static_cast<std::size_t>(kInvalidPriceLevelId), "PriceLevelPool is too large");

    PriceLevelPool() {
        for (PriceLevelId id = 0; id + 1 < Capacity; ++id) m_next_free[id] = id + 1;
        m_next_free[Capacity - 1] = kInvalidPriceLevelId;
    }

    PriceLevelId Allocate(Price price) {
        if (m_free_head == kInvalidPriceLevelId) return kInvalidPriceLevelId;
        const PriceLevelId id = m_free_head;
        m_free_head = m_next_free[id];
        m_levels[id] = PriceLevel{true, price};
        return id;
    }

    void Free(PriceLevelId id) {
        m_levels[id] = PriceLevel{};
        m_next_free[id] = m_free_head;
        m_free_head = id;
    }

    PriceLevel& Get(PriceLevelId id) { return m_levels[id]; }
    const PriceLevel& Get(PriceLevelId id) const { return m_levels[id]; }

private:
    std::array<PriceLevel, Capacity> m_levels{};
    std::array<PriceLevelId, Capacity> m_next_free{};
    PriceLevelId m_free_head = 0;
};

/**
 * Flat, sorted price-to-level index. It intentionally replaces a red-black
 * tree: binary search and contiguous storage are more cache-friendly for V1.
 */
template <std::size_t Capacity>
class PriceIndex {
public:
    static_assert(Capacity > 0, "PriceIndex requires at least one entry");

    PriceLevelId Find(Price price) const {
        const std::size_t position = LowerBound(price);
        return position < m_size && m_entries[position].price == price
                   ? m_entries[position].level_id
                   : kInvalidPriceLevelId;
    }

    bool Insert(Price price, PriceLevelId level_id) {
        if (m_size == Capacity) return false;
        const std::size_t position = LowerBound(price);
        if (position < m_size && m_entries[position].price == price) return false;
        for (std::size_t index = m_size; index > position; --index) m_entries[index] = m_entries[index - 1];
        m_entries[position] = {price, level_id};
        ++m_size;
        return true;
    }

    void Remove(Price price) {
        const std::size_t position = LowerBound(price);
        if (position == m_size || m_entries[position].price != price) return;
        for (std::size_t index = position + 1; index < m_size; ++index) m_entries[index - 1] = m_entries[index];
        --m_size;
    }

    PriceLevelId Best(Side side) const {
        if (m_size == 0) return kInvalidPriceLevelId;
        return side == Side::Buy ? m_entries[m_size - 1].level_id : m_entries[0].level_id;
    }

private:
    struct Entry {
        Price price = 0.0;
        PriceLevelId level_id = kInvalidPriceLevelId;
    };

    std::size_t LowerBound(Price price) const {
        std::size_t first = 0;
        std::size_t count = m_size;
        while (count > 0) {
            const std::size_t step = count / 2;
            const std::size_t position = first + step;
            if (m_entries[position].price < price) {
                first = position + 1;
                count -= step + 1;
            } else {
                count = step;
            }
        }
        return first;
    }

    std::array<Entry, Capacity> m_entries{};
    std::size_t m_size = 0;
};

/** One side of a symbol book: price index plus FIFO chains per price level. */
template <std::size_t MaxOrders, std::size_t MaxPriceLevels>
class BookSide {
public:
    using Pool = OrderPool<MaxOrders>;

    explicit BookSide(Side side) : m_side(side) {}

    bool Add(OrderIndex order_index, Pool& order_pool) {
        const Order& order = order_pool.Get(order_index);
        if (order.side != m_side) return false;

        PriceLevelId level_id = m_price_index.Find(order.price);
        if (level_id == kInvalidPriceLevelId) {
            level_id = m_price_level_pool.Allocate(order.price);
            if (level_id == kInvalidPriceLevelId || !m_price_index.Insert(order.price, level_id)) {
                if (level_id != kInvalidPriceLevelId) m_price_level_pool.Free(level_id);
                return false;
            }
        }
        AddToLevel(m_price_level_pool.Get(level_id), order_index, order_pool);
        return true;
    }

    void Remove(OrderIndex order_index, Pool& order_pool) {
        const Order& order = order_pool.Get(order_index);
        if (order.side != m_side) return;

        const PriceLevelId level_id = m_price_index.Find(order.price);
        if (level_id == kInvalidPriceLevelId) return;

        PriceLevel& level = m_price_level_pool.Get(level_id);
        RemoveFromLevel(level, order_index, order_pool);
        if (level.order_count == 0) {
            m_price_index.Remove(order.price);
            m_price_level_pool.Free(level_id);
        }
    }

    void ReduceQuantity(OrderIndex order_index, Quantity quantity, const Pool& order_pool) {
        const Order& order = order_pool.Get(order_index);
        if (order.side != m_side) return;

        const PriceLevelId level_id = m_price_index.Find(order.price);
        if (level_id != kInvalidPriceLevelId) m_price_level_pool.Get(level_id).total_quantity -= quantity;
    }

    std::optional<OrderIndex> BestOrder() const {
        const PriceLevelId level_id = m_price_index.Best(m_side);
        if (level_id == kInvalidPriceLevelId) return std::nullopt;
        return m_price_level_pool.Get(level_id).head;
    }

private:
    void AddToLevel(PriceLevel& level, OrderIndex order_index, Pool& order_pool) {
        Order& order = order_pool.Get(order_index);
        order.prev = level.tail;
        order.next = kInvalidOrderIndex;
        if (level.tail == kInvalidOrderIndex) level.head = order_index;
        else order_pool.Get(level.tail).next = order_index;
        level.tail = order_index;
        level.total_quantity += order.quantity;
        ++level.order_count;
    }

    void RemoveFromLevel(PriceLevel& level, OrderIndex order_index, Pool& order_pool) {
        Order& order = order_pool.Get(order_index);
        if (order.prev == kInvalidOrderIndex) level.head = order.next;
        else order_pool.Get(order.prev).next = order.next;
        if (order.next == kInvalidOrderIndex) level.tail = order.prev;
        else order_pool.Get(order.next).prev = order.prev;
        level.total_quantity -= order.quantity;
        --level.order_count;
    }

    Side m_side;
    PriceLevelPool<MaxPriceLevels> m_price_level_pool;
    PriceIndex<MaxPriceLevels> m_price_index;
};

/** Per-symbol order book with one buy side and one sell side. */
template <std::size_t MaxOrders, std::size_t MaxPriceLevels = 4'096>
class OrderBook {
public:
    using Pool = OrderPool<MaxOrders>;

    explicit OrderBook(std::string symbol = "DEFAULT")
        : m_symbol(std::move(symbol)),
          m_buy_side(Side::Buy),
          m_sell_side(Side::Sell) {}

    const std::string& symbol() const { return m_symbol; }

    bool Add(OrderIndex order_index, Pool& order_pool) {
        const Side side = order_pool.Get(order_index).side;
        return side == Side::Buy ? m_buy_side.Add(order_index, order_pool)
                                 : m_sell_side.Add(order_index, order_pool);
    }

    void Remove(OrderIndex order_index, Pool& order_pool) {
        const Side side = order_pool.Get(order_index).side;
        if (side == Side::Buy) m_buy_side.Remove(order_index, order_pool);
        else m_sell_side.Remove(order_index, order_pool);
    }

    void ReduceQuantity(OrderIndex order_index, Quantity quantity, const Pool& order_pool) {
        const Side side = order_pool.Get(order_index).side;
        if (side == Side::Buy) m_buy_side.ReduceQuantity(order_index, quantity, order_pool);
        else m_sell_side.ReduceQuantity(order_index, quantity, order_pool);
    }

    std::optional<OrderIndex> BestOrder(Side side) const {
        return side == Side::Buy ? m_buy_side.BestOrder() : m_sell_side.BestOrder();
    }

private:
    std::string m_symbol;
    BookSide<MaxOrders, MaxPriceLevels> m_buy_side;
    BookSide<MaxOrders, MaxPriceLevels> m_sell_side;
};

} // namespace nme
