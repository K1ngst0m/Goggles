#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winterference-size"
#include <rigtorp/SPSCQueue.h>
#pragma GCC diagnostic pop
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>

namespace goggles::util {

template <typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity) : m_queue(capacity) {
        // Note: rigtorp::SPSCQueue requires power-of-2 capacity
        if ((capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("SPSCQueue capacity must be power of 2");
        }
    }

    ~SPSCQueue() = default;

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    auto try_push(const T& item) -> bool {
        return m_queue.try_push(item);
    }

    auto try_push(T&& item) -> bool {
        return m_queue.try_push(std::move(item));
    }

    auto try_pop() -> std::optional<T> {
        T* front_item = m_queue.front();
        if (front_item) {
            T item = std::move(*front_item);
            m_queue.pop();
            return std::move(item);
        }
        return std::nullopt;
    }

    auto size() const -> size_t {
        return m_queue.size();
    }

    auto capacity() const -> size_t {
        return m_queue.capacity();
    }

private:
    rigtorp::SPSCQueue<T> m_queue;
};

} // namespace goggles::util
