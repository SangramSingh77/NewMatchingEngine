#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

namespace nme {

using OrderId = std::uint64_t;
using Quantity = std::uint64_t;
using Price = double;
using OrderIndex = std::uint32_t;

constexpr OrderIndex kInvalidOrderIndex = std::numeric_limits<OrderIndex>::max();

enum class Side : std::uint8_t { Buy = 0, Sell = 1 };

struct Order {
    bool active = false;
    OrderId id = 0;
    Side side = Side::Buy;
    Quantity quantity = 0;
    Price price = 0.0;
    OrderIndex next = kInvalidOrderIndex;
    OrderIndex prev = kInvalidOrderIndex;
};

/** Fixed-size order storage. Slots are recycled through an O(1) free list. */
template <std::size_t MaxOrders>
class OrderPool {
public:
    static_assert(MaxOrders > 0, "OrderPool requires at least one slot");
    static_assert(MaxOrders < static_cast<std::size_t>(kInvalidOrderIndex), "OrderPool is too large");

    OrderPool() {
        for (OrderIndex index = 0; index + 1 < MaxOrders; ++index) m_orders[index].next = index + 1;
        m_orders[MaxOrders - 1].next = kInvalidOrderIndex;
    }

    OrderIndex Allocate(const Order& order) {
        if (m_free_head == kInvalidOrderIndex) return kInvalidOrderIndex;
        const OrderIndex index = m_free_head;
        m_free_head = m_orders[index].next;
        m_orders[index] = order;
        m_orders[index].active = true;
        return index;
    }

    void Free(OrderIndex index) {
        m_orders[index] = Order{};
        m_orders[index].next = m_free_head;
        m_free_head = index;
    }

    Order& Get(OrderIndex index) { return m_orders[index]; }
    const Order& Get(OrderIndex index) const { return m_orders[index]; }

private:
    std::array<Order, MaxOrders> m_orders{};
    OrderIndex m_free_head = 0;
};

/** Fixed, allocation-free order-ID lookup table using linear probing. */
template <std::size_t Capacity>
class OrderManager {
public:
    static_assert(Capacity > 0, "OrderManager requires at least one slot");

    bool Add(OrderId id, OrderIndex index) {
        std::size_t deleted_slot = Capacity;
        for (std::size_t probe = 0; probe < Capacity; ++probe) {
            const std::size_t slot = GetSlot(id, probe);
            Entry& entry = m_entries[slot];
            if (entry.state == State::Empty) {
                m_entries[deleted_slot == Capacity ? slot : deleted_slot] = {id, index, State::Used};
                return true;
            }
            if (entry.state == State::Deleted && deleted_slot == Capacity) deleted_slot = slot;
            if (entry.state == State::Used && entry.id == id) return false;
        }
        if (deleted_slot != Capacity) {
            m_entries[deleted_slot] = {id, index, State::Used};
            return true;
        }
        return false;
    }

    OrderIndex Get(OrderId id) const {
        for (std::size_t probe = 0; probe < Capacity; ++probe) {
            const Entry& entry = m_entries[GetSlot(id, probe)];
            if (entry.state == State::Empty) return kInvalidOrderIndex;
            if (entry.state == State::Used && entry.id == id) return entry.index;
        }
        return kInvalidOrderIndex;
    }

    void Remove(OrderId id) {
        for (std::size_t probe = 0; probe < Capacity; ++probe) {
            Entry& entry = m_entries[GetSlot(id, probe)];
            if (entry.state == State::Empty) return;
            if (entry.state == State::Used && entry.id == id) {
                entry.state = State::Deleted;
                return;
            }
        }
    }

private:
    enum class State : std::uint8_t { Empty, Used, Deleted };
    struct Entry { OrderId id = 0; OrderIndex index = kInvalidOrderIndex; State state = State::Empty; };

    std::size_t GetSlot(OrderId id, std::size_t probe) const {
        return (std::hash<OrderId>{}(id) + probe) % Capacity;
    }

    std::array<Entry, Capacity> m_entries{};
};

} // namespace nme
