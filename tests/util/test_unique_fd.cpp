#include "util/unique_fd.hpp"

#include <catch2/catch_test_macros.hpp>
#include <fcntl.h>
#include <unistd.h>

using namespace goggles::util;

// Helper to check if an fd is valid (can be read from)
static bool is_fd_valid(int fd) {
    return fcntl(fd, F_GETFD) != -1;
}

// Helper to create a valid fd for testing using pipe()
static int create_test_fd() {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return -1;
    }
    // Close read end, return write end
    close(pipefd[0]);
    return pipefd[1];
}

TEST_CASE("UniqueFd construction", "[unique_fd]") {
    SECTION("Default construction is invalid") {
        UniqueFd fd;
        REQUIRE(!fd.valid());
        REQUIRE(!fd);
        REQUIRE(fd.get() == -1);
    }

    SECTION("Construction with valid fd") {
        int raw_fd = create_test_fd();
        REQUIRE(raw_fd >= 0);

        UniqueFd fd{raw_fd};
        REQUIRE(fd.valid());
        REQUIRE(fd);
        REQUIRE(fd.get() == raw_fd);
    }

    SECTION("Construction with -1 is invalid") {
        UniqueFd fd{-1};
        REQUIRE(!fd.valid());
        REQUIRE(!fd);
    }
}

TEST_CASE("UniqueFd destructor closes fd", "[unique_fd]") {
    int raw_fd = create_test_fd();
    REQUIRE(raw_fd >= 0);
    REQUIRE(is_fd_valid(raw_fd));

    {
        UniqueFd fd{raw_fd};
        REQUIRE(fd.valid());
    }
    // After scope exit, fd should be closed
    REQUIRE(!is_fd_valid(raw_fd));
}

TEST_CASE("UniqueFd move semantics", "[unique_fd]") {
    SECTION("Move constructor transfers ownership") {
        int raw_fd = create_test_fd();
        REQUIRE(raw_fd >= 0);

        UniqueFd source{raw_fd};
        UniqueFd dest{std::move(source)};

        REQUIRE(dest.valid());
        REQUIRE(dest.get() == raw_fd);
        REQUIRE(!source.valid());
        REQUIRE(source.get() == -1);
    }

    SECTION("Move assignment transfers ownership") {
        int fd1 = create_test_fd();
        int fd2 = create_test_fd();
        REQUIRE(fd1 >= 0);
        REQUIRE(fd2 >= 0);

        UniqueFd source{fd1};
        UniqueFd dest{fd2};

        dest = std::move(source);

        REQUIRE(dest.valid());
        REQUIRE(dest.get() == fd1);
        REQUIRE(!source.valid());
        // fd2 should have been closed by the assignment
        REQUIRE(!is_fd_valid(fd2));
    }

    SECTION("Self-assignment is safe") {
        int raw_fd = create_test_fd();
        REQUIRE(raw_fd >= 0);

        UniqueFd fd{raw_fd};

        // Test self-move using indirection to avoid compiler warning
        // This still tests the same runtime behavior (this == &other)
        UniqueFd* ptr = &fd;
        fd = std::move(*ptr);

        // Should still be valid after self-assignment
        REQUIRE(fd.valid());
        REQUIRE(fd.get() == raw_fd);
    }

    SECTION("Source becomes invalid after move") {
        int raw_fd = create_test_fd();
        REQUIRE(raw_fd >= 0);

        UniqueFd source{raw_fd};
        UniqueFd dest = std::move(source);

        REQUIRE(!source.valid());
        REQUIRE(source.get() == -1);
        // Destructor of source should be safe (no double-close)
    }
}

TEST_CASE("UniqueFd dup() creates independent copy", "[unique_fd]") {
    SECTION("Duplicated fd is valid") {
        int raw_fd = create_test_fd();
        REQUIRE(raw_fd >= 0);

        UniqueFd original{raw_fd};
        UniqueFd copy = original.dup();

        REQUIRE(copy.valid());
        REQUIRE(copy.get() != original.get()); // Different fd numbers
    }

    SECTION("Original remains valid after dup") {
        int raw_fd = create_test_fd();
        REQUIRE(raw_fd >= 0);

        UniqueFd original{raw_fd};
        UniqueFd copy = original.dup();

        REQUIRE(original.valid());
        REQUIRE(original.get() == raw_fd);
    }

    SECTION("Closing one doesn't affect other") {
        int raw_fd = create_test_fd();
        REQUIRE(raw_fd >= 0);

        UniqueFd original{raw_fd};
        UniqueFd copy = original.dup();
        int copy_fd = copy.get();

        // Destroy original
        original = UniqueFd{};

        // Copy should still be valid
        REQUIRE(copy.valid());
        REQUIRE(is_fd_valid(copy_fd));
    }

    SECTION("dup() on invalid fd returns invalid") {
        UniqueFd invalid;
        UniqueFd copy = invalid.dup();

        REQUIRE(!copy.valid());
        REQUIRE(copy.get() == -1);
    }
}

TEST_CASE("UniqueFd dup_from() wraps raw fd", "[unique_fd]") {
    SECTION("Creates valid UniqueFd from raw fd") {
        int raw_fd = create_test_fd();
        REQUIRE(raw_fd >= 0);

        UniqueFd fd = UniqueFd::dup_from(raw_fd);

        REQUIRE(fd.valid());
        REQUIRE(fd.get() != raw_fd);  // Should be a duplicate, not the same
        REQUIRE(is_fd_valid(raw_fd)); // Original still valid

        // Clean up original
        close(raw_fd);
    }

    SECTION("dup_from(-1) returns invalid") {
        UniqueFd fd = UniqueFd::dup_from(-1);
        REQUIRE(!fd.valid());
        REQUIRE(fd.get() == -1);
    }

    SECTION("dup_from() on closed fd returns invalid") {
        int raw_fd = create_test_fd();
        close(raw_fd);

        UniqueFd fd = UniqueFd::dup_from(raw_fd);
        REQUIRE(!fd.valid());
    }
}

TEST_CASE("UniqueFd release() transfers ownership out", "[unique_fd]") {
    SECTION("Returns fd and becomes invalid") {
        int raw_fd = create_test_fd();
        REQUIRE(raw_fd >= 0);

        UniqueFd fd{raw_fd};
        int released = fd.release();

        REQUIRE(released == raw_fd);
        REQUIRE(!fd.valid());
        REQUIRE(fd.get() == -1);

        // fd should still be valid (not closed)
        REQUIRE(is_fd_valid(released));

        // Clean up manually since we took ownership
        close(released);
    }

    SECTION("Release on invalid returns -1") {
        UniqueFd fd;
        int released = fd.release();

        REQUIRE(released == -1);
        REQUIRE(!fd.valid());
    }

    SECTION("Double release is safe") {
        int raw_fd = create_test_fd();
        REQUIRE(raw_fd >= 0);

        UniqueFd fd{raw_fd};
        int first = fd.release();
        int second = fd.release();

        REQUIRE(first == raw_fd);
        REQUIRE(second == -1);

        close(first);
    }
}

TEST_CASE("UniqueFd get() and valid()", "[unique_fd]") {
    SECTION("get() returns underlying fd") {
        int raw_fd = create_test_fd();
        REQUIRE(raw_fd >= 0);

        UniqueFd fd{raw_fd};
        REQUIRE(fd.get() == raw_fd);
    }

    SECTION("get() on invalid returns -1") {
        UniqueFd fd;
        REQUIRE(fd.get() == -1);
    }

    SECTION("valid() matches get() >= 0") {
        UniqueFd invalid;
        REQUIRE(!invalid.valid());
        REQUIRE(invalid.get() < 0);

        int raw_fd = create_test_fd();
        UniqueFd valid_fd{raw_fd};
        REQUIRE(valid_fd.valid());
        REQUIRE(valid_fd.get() >= 0);
    }

    SECTION("bool conversion matches valid()") {
        UniqueFd invalid;
        REQUIRE(static_cast<bool>(invalid) == invalid.valid());
        REQUIRE(!static_cast<bool>(invalid));

        int raw_fd = create_test_fd();
        UniqueFd valid_fd{raw_fd};
        REQUIRE(static_cast<bool>(valid_fd) == valid_fd.valid());
        REQUIRE(static_cast<bool>(valid_fd));
    }
}
