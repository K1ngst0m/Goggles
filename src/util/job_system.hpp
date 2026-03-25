#pragma once

#include <BS_thread_pool.hpp>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>

namespace goggles::util {

/// @brief Global thread pool for lightweight background jobs.
class JobSystem {
public:
    /// `thread_count = 0` uses `std::thread::hardware_concurrency()`. Idempotent.
    static void initialize(size_t thread_count = 0);
    static void shutdown();

    /// Lazily initializes the pool if needed.
    template <typename Func, typename... Args>
    static auto submit(Func&& func, Args&&... args)
        -> std::future<std::invoke_result_t<Func, Args...>> {
        ensure_initialized();
        return s_pool->submit(std::forward<Func>(func), std::forward<Args>(args)...);
    }

    static void wait_all();
    /// Returns `1` if the pool is not yet initialized.
    static auto thread_count() -> size_t;
    static auto is_initialized() -> bool { return s_pool != nullptr; }

    JobSystem() = delete;
    ~JobSystem() = delete;
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

private:
    static void ensure_initialized();
    static std::unique_ptr<BS::thread_pool> s_pool;
};

} // namespace goggles::util
