#pragma once

// Lock-Free Queue Interfaces
// Status: Skeleton (to be implemented when threading is needed)
// See: docs/threading_architecture.md for design rationale

#include <cstddef>
#include <memory>

namespace goggles::util {

template <typename T>
class SPSCQueue {
public:
    explicit SPSCQueue([[maybe_unused]] size_t capacity) {
#ifdef GOGGLES_THREADING_ENABLED
        // TODO: Implement rigtorp::SPSCQueue construction
#endif
    }

    ~SPSCQueue() = default;

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    auto try_push([[maybe_unused]] const T& item) -> bool {
#ifdef GOGGLES_THREADING_ENABLED
        // TODO: Implement rigtorp::SPSCQueue::try_push
        return false;
#else
        return false;
#endif
    }

    auto try_push([[maybe_unused]] T&& item) -> bool {
#ifdef GOGGLES_THREADING_ENABLED
        // TODO: Implement rigtorp::SPSCQueue::try_push
        return false;
#else
        return false;
#endif
    }

    auto try_pop() -> T* {
#ifdef GOGGLES_THREADING_ENABLED
        // TODO: Implement rigtorp::SPSCQueue::try_pop
        return nullptr;
#else
        return nullptr;
#endif
    }

    auto size() const -> size_t {
#ifdef GOGGLES_THREADING_ENABLED
        // TODO: Implement rigtorp::SPSCQueue::size
        return 0;
#else
        return 0;
#endif
    }

    auto capacity() const -> size_t {
#ifdef GOGGLES_THREADING_ENABLED
        // TODO: Implement rigtorp::SPSCQueue::capacity
        return 0;
#else
        return 1;
#endif
    }

private:
#ifdef GOGGLES_THREADING_ENABLED
    struct Impl;
    std::unique_ptr<Impl> m_impl;
#endif
};

} // namespace goggles::util
