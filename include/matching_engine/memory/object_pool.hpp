#pragma once
#include "../domain/order.hpp"
#include <vector>
#include <optional>

namespace me::memory {

template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t capacity = 1024) { pool_.reserve(capacity); }
    T* allocate() { pool_.emplace_back(); return &pool_.back(); }
    void deallocate(T* /*ptr*/) { /* noop for simple arena */ }

    void clear() { pool_.clear(); }

private:
    std::vector<T> pool_;
};

} // namespace me::memory
