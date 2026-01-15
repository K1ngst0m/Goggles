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
    /// @brief Initializes the global worker pool (idempotent).
    /// @param thread_count Thread count; `0` uses `std::thread::hardware_concurrency()`.
    static void initialize(size_t thread_count = 0);
    /// @brief Waits for all tasks and destroys the global worker pool.
    static void shutdown();

    /// @brief Submits a job to the pool, initializing it if needed.
    /// @param func Callable to execute on the pool.
    /// @param args Arguments forwarded to `func`.
    /// @return A future for the job result.
    template <typename Func, typename... Args>
    static auto submit(Func&& func, Args&&... args)
        -> std::future<std::invoke_result_t<Func, Args...>> {
        ensure_initialized();
        return s_pool->submit(std::forward<Func>(func), std::forward<Args>(args)...);
    }

    /// @brief Blocks until all currently queued tasks complete.
    static void wait_all();
    /// @brief Returns the pool size; returns `1` if uninitialized.
    static auto thread_count() -> size_t;
    /// @brief Returns true if the pool has been initialized.
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
