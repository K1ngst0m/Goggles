#include "../../src/util/queues.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace goggles::util;

TEST_CASE("SPSCQueue construction and basic properties", "[queues]") {
    SECTION("Construct with valid capacity") {
        SPSCQueue<int> queue(8);
        REQUIRE(queue.capacity() == 8);
        REQUIRE(queue.size() == 0);
    }

    SECTION("Non-power-of-2 capacity is accepted") {
        SPSCQueue<int> queue(7);
        REQUIRE(queue.capacity() == 7);

        SPSCQueue<int> queue2(10);
        REQUIRE(queue2.capacity() == 10);
    }

    SECTION("Minimum capacity of 1 works") {
        SPSCQueue<int> queue(1);
        REQUIRE(queue.capacity() == 1);
    }

    SECTION("Zero capacity is rejected") {
        REQUIRE_THROWS_AS(SPSCQueue<int>(0), std::invalid_argument);
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
            bool operator==(const TestStruct& other) const { return x == other.x && y == other.y; }
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

TEST_CASE("SPSCQueue edge cases and boundary conditions", "[queues]") {
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
}
