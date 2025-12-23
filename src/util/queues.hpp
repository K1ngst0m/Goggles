#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace goggles::util {

template <typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity)
        : m_capacity(capacity), m_buffer_size(capacity * 2), m_capacity_mask(m_buffer_size - 1),
          m_buffer(nullptr) {
        // Power-of-2 required for efficient modulo via bitwise AND
        if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("SPSCQueue capacity must be power of 2");
        }

        m_buffer = static_cast<T*>(std::aligned_alloc(alignof(T), sizeof(T) * m_buffer_size));
        if (m_buffer == nullptr) {
            throw std::bad_alloc();
        }

        m_head.store(0, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_relaxed);
    }

    ~SPSCQueue() {
        while (try_pop()) {
        }
        if (m_buffer) {
            std::free(m_buffer);
        }
    }

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    auto try_push(const T& item) -> bool {
        const size_t current_head = m_head.load(std::memory_order_relaxed);
        const size_t current_tail = m_tail.load(std::memory_order_acquire);
        const size_t current_size = (current_head - current_tail) & m_capacity_mask;
        if (current_size >= m_capacity) {
            return false;
        }

        new (&m_buffer[current_head]) T(item);
        const size_t next_head = (current_head + 1) & m_capacity_mask;
        m_head.store(next_head, std::memory_order_release);
        return true;
    }

    auto try_push(T&& item) -> bool {
        const size_t current_head = m_head.load(std::memory_order_relaxed);
        const size_t current_tail = m_tail.load(std::memory_order_acquire);
        const size_t current_size = (current_head - current_tail) & m_capacity_mask;
        if (current_size >= m_capacity) {
            return false;
        }

        new (&m_buffer[current_head]) T(std::move(item));
        const size_t next_head = (current_head + 1) & m_capacity_mask;
        m_head.store(next_head, std::memory_order_release);
        return true;
    }

    auto try_pop() -> std::optional<T> {
        const size_t current_tail = m_tail.load(std::memory_order_relaxed);
        if (current_tail == m_head.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        T item = std::move(m_buffer[current_tail]);
        m_buffer[current_tail].~T();
        const size_t next_tail = (current_tail + 1) & m_capacity_mask;
        m_tail.store(next_tail, std::memory_order_release);
        return item;
    }

    [[nodiscard]] auto size() const -> size_t {
        const size_t current_head = m_head.load(std::memory_order_acquire);
        const size_t current_tail = m_tail.load(std::memory_order_acquire);
        return (current_head - current_tail) & m_capacity_mask;
    }

    [[nodiscard]] auto empty() const -> bool {
        const size_t current_tail = m_tail.load(std::memory_order_relaxed);
        return current_tail == m_head.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto capacity() const -> size_t { return m_capacity; }

private:
    alignas(64) std::atomic<size_t> m_head;
    alignas(64) std::atomic<size_t> m_tail;

    const size_t m_capacity;
    const size_t m_buffer_size;
    const size_t m_capacity_mask;
    T* m_buffer;
};

} // namespace goggles::util
