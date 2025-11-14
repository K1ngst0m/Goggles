#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <thread>
#include "../../src/util/job_system.hpp"

using namespace goggles::util;

TEST_CASE("JobSystem initialization and shutdown", "[job_system]") {
    SECTION("Initialize with default thread count") {
        JobSystem::initialize();
        REQUIRE(JobSystem::is_initialized());
        REQUIRE(JobSystem::thread_count() > 0);
        JobSystem::shutdown();
        REQUIRE_FALSE(JobSystem::is_initialized());
    }

    SECTION("Initialize with specific thread count") {
        JobSystem::initialize(4);
        REQUIRE(JobSystem::is_initialized());
        REQUIRE(JobSystem::thread_count() == 4);
        JobSystem::shutdown();
        REQUIRE_FALSE(JobSystem::is_initialized());
    }

    SECTION("Multiple initialization calls are safe") {
        JobSystem::initialize(2);
        auto first_count = JobSystem::thread_count();
        
        JobSystem::initialize(4); // Should be ignored
        auto second_count = JobSystem::thread_count();
        
        REQUIRE(first_count == second_count);
        JobSystem::shutdown();
    }
}

TEST_CASE("JobSystem job submission and execution", "[job_system]") {
    JobSystem::initialize(2);

    SECTION("Submit and execute simple job") {
        std::atomic<bool> job_executed{false};
        
        auto future = JobSystem::submit([&job_executed]() {
            job_executed = true;
        });
        
        future.wait();
        REQUIRE(job_executed.load());
    }

    SECTION("Submit job with return value") {
        auto future = JobSystem::submit([]() -> int {
            return 42;
        });
        
        REQUIRE(future.get() == 42);
    }

    SECTION("Submit job with parameters") {
        auto future = JobSystem::submit([](int a, int b) -> int {
            return a + b;
        }, 10, 20);
        
        REQUIRE(future.get() == 30);
    }

    SECTION("Submit multiple jobs") {
        std::atomic<int> counter{0};
        std::vector<std::future<void>> futures;
        
        for (int i = 0; i < 10; ++i) {
            auto future = JobSystem::submit([&counter]() {
                counter++;
            });
            futures.push_back(std::move(future));
        }
        
        for (auto& future : futures) {
            future.wait();
        }
        
        REQUIRE(counter.load() == 10);
    }

    JobSystem::shutdown();
}

TEST_CASE("JobSystem wait_all functionality", "[job_system]") {
    JobSystem::initialize(2);

    SECTION("wait_all waits for all submitted jobs") {
        std::atomic<int> completed_jobs{0};
        
        // Submit several jobs that take some time
        for (int i = 0; i < 5; ++i) {
            JobSystem::submit([&completed_jobs]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                completed_jobs++;
            });
        }
        
        // Wait for all jobs to complete
        JobSystem::wait_all();
        
        // All jobs should be completed by now
        REQUIRE(completed_jobs.load() == 5);
    }

    JobSystem::shutdown();
}

TEST_CASE("JobSystem automatic initialization", "[job_system]") {
    SECTION("Submit job without explicit initialization") {
        // JobSystem should auto-initialize
        REQUIRE_FALSE(JobSystem::is_initialized());
        
        auto future = JobSystem::submit([]() -> int {
            return 123;
        });
        
        REQUIRE(JobSystem::is_initialized());
        REQUIRE(future.get() == 123);
        
        JobSystem::shutdown();
    }
}

TEST_CASE("JobSystem concurrent access", "[job_system]") {
    JobSystem::initialize(4);

    SECTION("Multiple threads can submit jobs concurrently") {
        std::atomic<int> total_executed{0};
        std::vector<std::thread> submitter_threads;
        
        // Create multiple threads that submit jobs
        for (int t = 0; t < 3; ++t) {
            submitter_threads.emplace_back([&total_executed]() {
                std::vector<std::future<void>> futures;
                
                for (int i = 0; i < 10; ++i) {
                    auto future = JobSystem::submit([&total_executed]() {
                        total_executed++;
                    });
                    futures.push_back(std::move(future));
                }
                
                for (auto& future : futures) {
                    future.wait();
                }
            });
        }
        
        // Wait for all submitter threads
        for (auto& thread : submitter_threads) {
            thread.join();
        }
        
        REQUIRE(total_executed.load() == 30); // 3 threads * 10 jobs each
    }

    JobSystem::shutdown();
}

TEST_CASE("JobSystem performance characteristics", "[job_system]") {
    JobSystem::initialize(4);

    SECTION("Job dispatch overhead is reasonable") {
        const int num_jobs = 1000;
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::future<void>> futures;
        futures.reserve(num_jobs);
        
        for (int i = 0; i < num_jobs; ++i) {
            futures.push_back(JobSystem::submit([]() {
                // Minimal work
            }));
        }
        
        for (auto& future : futures) {
            future.wait();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        auto avg_time_per_job = total_time.count() / num_jobs;
        
        // Each job should take less than 100 microseconds on average (very generous)
        REQUIRE(avg_time_per_job < 100);
    }

    JobSystem::shutdown();
}

TEST_CASE("JobSystem exception handling", "[job_system]") {
    JobSystem::initialize(2);

    SECTION("Jobs that throw exceptions don't crash the system") {
        auto future = JobSystem::submit([]() -> int {
            throw std::runtime_error("Test exception");
            return 42; // Never reached
        });
        
        REQUIRE_THROWS_AS(future.get(), std::runtime_error);
        
        // System should still work after exception
        auto another_future = JobSystem::submit([]() -> int {
            return 123;
        });
        
        REQUIRE(another_future.get() == 123);
    }

    JobSystem::shutdown();
}
