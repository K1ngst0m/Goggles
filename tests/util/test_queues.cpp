#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include "../../src/util/queues.hpp"

using namespace goggles::util;

TEST_CASE("SPSCQueue construction and basic properties", "[queues]") {
    SECTION("Construct with power-of-2 capacity") {
        SPSCQueue<int> queue(8);
        REQUIRE(queue.capacity() == 8);
        REQUIRE(queue.size() == 0);
    }

    SECTION("Construct with non-power-of-2 capacity throws") {
        REQUIRE_THROWS_AS(SPSCQueue<int>(7), std::invalid_argument);
        REQUIRE_THROWS_AS(SPSCQueue<int>(10), std::invalid_argument);
    }

    SECTION("Minimum capacity of 1 works") {
        SPSCQueue<int> queue(1);
        REQUIRE(queue.capacity() == 1);
    }
}

TEST_CASE("SPSCQueue basic operations", "[queues]") {
    SPSCQueue<int> queue(4);

    SECTION("Push and pop single item") {
        REQUIRE(queue.try_push(42));
        REQUIRE(queue.size() == 1);

        auto result = queue.try_pop();
        REQUIRE(result.has_value());
        REQUIRE(*result == 42);
        REQUIRE(queue.size() == 0);
    }

    SECTION("Pop from empty queue returns nullopt") {
        auto result = queue.try_pop();
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Push to full queue returns false") {
        // Fill the queue
        for (size_t i = 0; i < queue.capacity(); ++i) {
            REQUIRE(queue.try_push(static_cast<int>(i)));
        }

        // Next push should fail
        REQUIRE_FALSE(queue.try_push(999));
    }
}

TEST_CASE("SPSCQueue move semantics", "[queues]") {
    SPSCQueue<std::unique_ptr<int>> queue(4);

    SECTION("Push and pop with move semantics") {
        auto ptr = std::make_unique<int>(42);
        int* raw_ptr = ptr.get();

        REQUIRE(queue.try_push(std::move(ptr)));
        REQUIRE(ptr == nullptr); // Moved from

        auto result = queue.try_pop();
        REQUIRE(result.has_value());
        REQUIRE(result->get() == raw_ptr);
        REQUIRE(**result == 42);
    }
}

TEST_CASE("SPSCQueue with different types", "[queues]") {
    SECTION("String queue") {
        SPSCQueue<std::string> queue(4);
        
        REQUIRE(queue.try_push("hello"));
        REQUIRE(queue.try_push(std::string("world")));

        auto first = queue.try_pop();
        auto second = queue.try_pop();

        REQUIRE(first.has_value());
        REQUIRE(second.has_value());
        REQUIRE(*first == "hello");
        REQUIRE(*second == "world");
    }

    SECTION("Struct queue") {
        struct TestStruct {
            int x, y;
            bool operator==(const TestStruct& other) const {
                return x == other.x && y == other.y;
            }
        };

        SPSCQueue<TestStruct> queue(2);
        TestStruct item{10, 20};

        REQUIRE(queue.try_push(item));
        
        auto result = queue.try_pop();
        REQUIRE(result.has_value());
        REQUIRE(*result == item);
    }
}

TEST_CASE("SPSCQueue capacity and size tracking", "[queues]") {
    SPSCQueue<int> queue(8);

    SECTION("Size increases with pushes") {
        REQUIRE(queue.size() == 0);
        
        queue.try_push(1);
        REQUIRE(queue.size() == 1);
        
        queue.try_push(2);
        REQUIRE(queue.size() == 2);
    }

    SECTION("Size decreases with pops") {
        queue.try_push(1);
        queue.try_push(2);
        REQUIRE(queue.size() == 2);

        queue.try_pop();
        REQUIRE(queue.size() == 1);

        queue.try_pop();
        REQUIRE(queue.size() == 0);
    }

    SECTION("Size is accurate when full") {
        for (size_t i = 0; i < queue.capacity(); ++i) {
            queue.try_push(static_cast<int>(i));
        }
        
        REQUIRE(queue.size() == queue.capacity());
    }
}

TEST_CASE("SPSCQueue FIFO ordering", "[queues]") {
    SPSCQueue<int> queue(8);

    SECTION("Items are retrieved in FIFO order") {
        std::vector<int> pushed_items = {1, 2, 3, 4, 5};
        
        // Push items
        for (int item : pushed_items) {
            REQUIRE(queue.try_push(item));
        }

        // Pop items and verify order
        for (size_t i = 0; i < pushed_items.size(); ++i) {
            auto result = queue.try_pop();
            REQUIRE(result.has_value());
            REQUIRE(*result == pushed_items[i]);
        }
    }
}

TEST_CASE("SPSCQueue single-threaded stress test", "[queues]") {
    SPSCQueue<int> queue(16);

    SECTION("Many push/pop cycles") {
        const int iterations = 1000;
        
        for (int i = 0; i < iterations; ++i) {
            REQUIRE(queue.try_push(i));
            auto result = queue.try_pop();
            REQUIRE(result.has_value());
            REQUIRE(*result == i);
        }
        
        REQUIRE(queue.size() == 0);
    }

    SECTION("Fill and empty cycles") {
        const int cycles = 100;
        
        for (int cycle = 0; cycle < cycles; ++cycle) {
            // Fill queue
            for (size_t i = 0; i < queue.capacity(); ++i) {
                REQUIRE(queue.try_push(static_cast<int>(cycle * 100 + i)));
            }
            
            REQUIRE(queue.size() == queue.capacity());
            
            // Empty queue
            for (size_t i = 0; i < queue.capacity(); ++i) {
                auto result = queue.try_pop();
                REQUIRE(result.has_value());
                REQUIRE(*result == static_cast<int>(cycle * 100 + i));
            }
            
            REQUIRE(queue.size() == 0);
        }
    }
}

TEST_CASE("SPSCQueue multi-threaded producer-consumer", "[queues]") {
    SPSCQueue<int> queue(64);  // Larger queue for multi-threaded test

    SECTION("Single producer, single consumer") {
        const int num_items = 1000;
        std::atomic<bool> producer_done{false};
        std::atomic<int> items_consumed{0};

        // Producer thread
        std::thread producer([&queue, num_items, &producer_done]() {
            for (int i = 0; i < num_items; ++i) {
                while (!queue.try_push(i)) {
                    std::this_thread::yield();  // Spin until we can push
                }
            }
            producer_done = true;
        });

        // Consumer thread
        std::thread consumer([&queue, &producer_done, &items_consumed]() {
            int consumed = 0;
            while (!producer_done.load() || queue.size() > 0) {
                auto result = queue.try_pop();
                if (result.has_value()) {
                    REQUIRE(*result == consumed);  // Verify FIFO order
                    consumed++;
                }
                else {
                    std::this_thread::yield();  // Queue empty, yield briefly
                }
            }
            items_consumed = consumed;
        });

        producer.join();
        consumer.join();

        REQUIRE(items_consumed.load() == num_items);
        REQUIRE(queue.size() == 0);
    }
}

TEST_CASE("SPSCQueue performance characteristics", "[queues]") {
    SPSCQueue<int> queue(1024);  // Large queue for performance test

    SECTION("Push/pop operations are fast") {
        const int num_operations = 10000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Interleaved push/pop operations
        for (int i = 0; i < num_operations; ++i) {
            queue.try_push(i);
            auto result = queue.try_pop();
            REQUIRE(result.has_value());
            REQUIRE(*result == i);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        auto avg_ns_per_op = duration.count() / (num_operations * 2);  // 2 ops per iteration
        
        // Each operation (push or pop) should take less than 1000ns on average
        REQUIRE(avg_ns_per_op < 1000);
    }
}

TEST_CASE("SPSCQueue with pointer types for zero-copy patterns", "[queues]") {
    struct FrameData {
        uint64_t id;
        std::vector<uint8_t> data;
    };

    // Pre-allocated frame buffers
    std::vector<std::unique_ptr<FrameData>> frame_buffers;
    for (int i = 0; i < 4; ++i) {
        auto frame = std::make_unique<FrameData>();
        frame->id = i;
        frame->data.resize(1024);  // Simulate frame data
        frame_buffers.push_back(std::move(frame));
    }

    SPSCQueue<FrameData*> queue(8);

    SECTION("Pass pointers to pre-allocated buffers") {
        // Producer: Submit pointers to frame buffers
        for (auto& frame_buffer : frame_buffers) {
            REQUIRE(queue.try_push(frame_buffer.get()));
        }

        // Consumer: Process frames and verify
        for (size_t expected_id = 0; expected_id < frame_buffers.size(); ++expected_id) {
            auto result = queue.try_pop();
            REQUIRE(result.has_value());
            
            FrameData* frame = *result;
            REQUIRE(frame->id == expected_id);
            REQUIRE(frame->data.size() == 1024);
        }
    }
}

TEST_CASE("SPSCQueue edge cases and boundary conditions", "[queues]") {
    SECTION("Zero capacity is rejected") {
        REQUIRE_THROWS_AS(SPSCQueue<int>(0), std::invalid_argument);
    }

    SECTION("Capacity 1 handles full/empty correctly") {
        SPSCQueue<int> queue(1);
        
        REQUIRE(queue.size() == 0);
        REQUIRE(queue.try_push(42));
        REQUIRE(queue.size() == 1);
        REQUIRE_FALSE(queue.try_push(43)); // Should fail when full
        
        auto result = queue.try_pop();
        REQUIRE(result.has_value());
        REQUIRE(*result == 42);
        REQUIRE(queue.size() == 0);
        
        auto empty_result = queue.try_pop();
        REQUIRE_FALSE(empty_result.has_value());
    }

    SECTION("Large capacity queue wraps around correctly") {
        SPSCQueue<int> queue(4);
        
        // Fill queue completely
        for (int i = 0; i < 4; ++i) {
            REQUIRE(queue.try_push(i));
        }
        REQUIRE_FALSE(queue.try_push(999)); // Should fail when full
        
        // Empty half
        for (int i = 0; i < 2; ++i) {
            auto result = queue.try_pop();
            REQUIRE(result.has_value());
            REQUIRE(*result == i);
        }
        
        // Fill again to test wrap-around
        for (int i = 100; i < 102; ++i) {
            REQUIRE(queue.try_push(i));
        }
        
        // Verify correct order with wrap-around
        std::vector<int> expected = {2, 3, 100, 101};
        for (int expected_val : expected) {
            auto result = queue.try_pop();
            REQUIRE(result.has_value());
            REQUIRE(*result == expected_val);
        }
    }
}

TEST_CASE("SPSCQueue memory ordering stress test", "[queues]") {
    SECTION("High contention producer-consumer") {
        SPSCQueue<int> queue(32);
        const int num_items = 10000;
        std::atomic<bool> test_failed{false};
        std::atomic<int> items_produced{0};
        std::atomic<int> items_consumed{0};
        
        // Producer thread - aggressive pushing
        std::thread producer([&]() {
            for (int i = 0; i < num_items; ++i) {
                while (!queue.try_push(i)) {
                    // Busy wait to create high contention
                    for (int j = 0; j < 10; ++j) {
                        asm volatile("" ::: "memory"); // Prevent optimization
                    }
                }
                items_produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
        
        // Consumer thread - aggressive popping
        std::thread consumer([&]() {
            int expected = 0;
            while (expected < num_items) {
                auto result = queue.try_pop();
                if (result.has_value()) {
                    if (*result != expected) {
                        test_failed = true;
                        break;
                    }
                    expected++;
                    items_consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    // Busy wait to create high contention
                    for (int j = 0; j < 10; ++j) {
                        asm volatile("" ::: "memory"); // Prevent optimization
                    }
                }
            }
        });
        
        producer.join();
        consumer.join();
        
        REQUIRE_FALSE(test_failed.load());
        REQUIRE(items_produced.load() == num_items);
        REQUIRE(items_consumed.load() == num_items);
        REQUIRE(queue.size() == 0);
    }
}

// Test helper for complex types testing
struct Resource {
    static std::atomic<int> instances;
    int id;
    
    Resource(int i) : id(i) { instances.fetch_add(1); }
    Resource(const Resource& other) : id(other.id) { instances.fetch_add(1); }
    Resource(Resource&& other) noexcept : id(other.id) { instances.fetch_add(1); }
    Resource& operator=(const Resource&) = default;
    Resource& operator=(Resource&&) noexcept = default;
    ~Resource() { instances.fetch_sub(1); }
};
std::atomic<int> Resource::instances{0};

TEST_CASE("SPSCQueue with complex types", "[queues]") {
    
    SECTION("Non-trivial types with destructors") {
        Resource::instances.store(0);
        
        {
            SPSCQueue<Resource> queue(4);
            
            // Push some resources
            queue.try_push(Resource(1));
            queue.try_push(Resource(2));
            REQUIRE(Resource::instances.load() == 2);
            
            // Pop one resource
            auto result = queue.try_pop();
            REQUIRE(result.has_value());
            REQUIRE(result->id == 1);
            
            // Resource should still exist until result goes out of scope
            REQUIRE(Resource::instances.load() == 2);
        }
        
        // All resources should be destroyed
        REQUIRE(Resource::instances.load() == 0);
    }
    
    SECTION("Types with custom move semantics") {
        struct MoveOnlyType {
            int value;
            bool moved_from = false;
            
            MoveOnlyType(int v) : value(v) {}
            MoveOnlyType(const MoveOnlyType&) = delete;
            MoveOnlyType& operator=(const MoveOnlyType&) = delete;
            
            MoveOnlyType(MoveOnlyType&& other) noexcept 
                : value(other.value), moved_from(false) {
                other.moved_from = true;
            }
            
            MoveOnlyType& operator=(MoveOnlyType&& other) noexcept {
                if (this != &other) {
                    value = other.value;
                    moved_from = false;
                    other.moved_from = true;
                }
                return *this;
            }
        };
        
        SPSCQueue<MoveOnlyType> queue(4);
        
        MoveOnlyType item(42);
        REQUIRE(queue.try_push(std::move(item)));
        REQUIRE(item.moved_from); // Should be moved from
        
        auto result = queue.try_pop();
        REQUIRE(result.has_value());
        REQUIRE(result->value == 42);
        REQUIRE_FALSE(result->moved_from);
    }
}
