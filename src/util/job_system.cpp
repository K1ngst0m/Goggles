#include "job_system.hpp"
#include <thread>

namespace goggles::util {

std::unique_ptr<BS::thread_pool> JobSystem::s_pool = nullptr;

void JobSystem::initialize(size_t thread_count) {
    if (s_pool) {
        return; // Already initialized
    }
    
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
    }
    
    s_pool = std::make_unique<BS::thread_pool>(thread_count);
}

void JobSystem::shutdown() {
    if (s_pool) {
        s_pool->wait_for_tasks();
        s_pool.reset();
    }
}

void JobSystem::wait_all() {
    if (s_pool) {
        s_pool->wait_for_tasks();
    }
}

auto JobSystem::thread_count() -> size_t {
    if (s_pool) {
        return s_pool->get_thread_count();
    }
    return 1; // Single-threaded fallback
}

void JobSystem::ensure_initialized() {
    if (!s_pool) {
        initialize(); // Initialize with default thread count
    }
}

} // namespace goggles::util
