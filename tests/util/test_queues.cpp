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
