#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

namespace goggles::util {

/// @brief Single-producer, single-consumer queue with fixed capacity.
///
/// Capacity must be > 0. Construction throws `std::invalid_argument` on zero capacity.
template <typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity)
        : m_capacity(capacity), m_buffer(capacity), m_head(0), m_tail(0), m_size(0) {
        if (capacity == 0) {
            throw std::invalid_argument("SPSCQueue capacity must be > 0");
        }
    }

    ~SPSCQueue() = default;

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    auto try_push(const T& item) -> bool {
        std::lock_guard lock(m_mutex);
        if (m_size >= m_capacity) {
            return false;
        }
        m_buffer[m_head].emplace(item);
        m_head = (m_head + 1) % m_capacity;
        ++m_size;
        return true;
    }

    auto try_push(T&& item) -> bool {
        std::lock_guard lock(m_mutex);
        if (m_size >= m_capacity) {
            return false;
        }
        m_buffer[m_head].emplace(std::move(item));
        m_head = (m_head + 1) % m_capacity;
        ++m_size;
        return true;
    }

    auto try_pop() -> std::optional<T> {
        std::lock_guard lock(m_mutex);
        if (m_size == 0) {
            return std::nullopt;
        }
        std::optional<T> item = std::move(m_buffer[m_tail]);
        m_buffer[m_tail].reset();
        m_tail = (m_tail + 1) % m_capacity;
        --m_size;
        return item;
    }

    [[nodiscard]] auto size() const -> size_t {
        std::lock_guard lock(m_mutex);
        return m_size;
    }

    [[nodiscard]] auto empty() const -> bool {
        std::lock_guard lock(m_mutex);
        return m_size == 0;
    }

    [[nodiscard]] auto capacity() const -> size_t { return m_capacity; }

private:
    const size_t m_capacity;
    std::vector<std::optional<T>> m_buffer;
    size_t m_head;
    size_t m_tail;
    size_t m_size;
    mutable std::mutex m_mutex;
};

} // namespace goggles::util
