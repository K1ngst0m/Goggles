#pragma once

#include <BS_thread_pool.hpp>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>

namespace goggles::util {

class JobSystem {
public:
    static void initialize(size_t thread_count = 0);
    static void shutdown();

    template <typename Func, typename... Args>
    static auto submit(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>> {
        ensure_initialized();
        return s_pool->submit(std::forward<Func>(func), std::forward<Args>(args)...);
    }

    static void wait_all();
    static auto thread_count() -> size_t;
    static auto is_initialized() -> bool { return s_pool != nullptr; }

private:
    JobSystem() = delete;
    ~JobSystem() = delete;
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    static void ensure_initialized();
    static std::unique_ptr<BS::thread_pool> s_pool;
};

} // namespace goggles::util
