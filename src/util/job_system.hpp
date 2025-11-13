#pragma once

// Threading & Job System Interface
// Status: Skeleton (to be implemented when threading is needed)
// See: docs/threading_architecture.md for design rationale

#include <cstddef>
#include <functional>
#include <future>

namespace goggles::util {

class JobSystem {
public:
    static void initialize([[maybe_unused]] size_t thread_count = 0) {
#ifdef GOGGLES_THREADING_ENABLED
        // TODO: Implement BS::thread_pool initialization
#endif
    }

    static void shutdown() {
#ifdef GOGGLES_THREADING_ENABLED
        // TODO: Implement thread pool shutdown
#endif
    }

    template <typename Func, typename... Args>
    static auto submit([[maybe_unused]] Func&& func, [[maybe_unused]] Args&&... args) -> std::future<void> {
#ifdef GOGGLES_THREADING_ENABLED
        // TODO: Implement job submission
        return std::future<void>();
#else
        std::promise<void> promise;
        promise.set_value();
        return promise.get_future();
#endif
    }

    static void wait_all() {
#ifdef GOGGLES_THREADING_ENABLED
        // TODO: Implement wait for all jobs
#endif
    }

    static auto thread_count() -> size_t {
#ifdef GOGGLES_THREADING_ENABLED
        // TODO: Return actual thread count
        return 0;
#else
        return 1;
#endif
    }

private:
    JobSystem() = delete;
    ~JobSystem() = delete;
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;
};

} // namespace goggles::util
