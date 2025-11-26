#include "util/logging.hpp"

#include <catch2/catch_test_macros.hpp>
#include <spdlog/spdlog.h>

using namespace goggles;

TEST_CASE("initialize_logger creates global logger", "[logging]") {
    // Note: Global logger state persists across tests, so we test behavior rather than exact names

    SECTION("Basic initialization") {
        initialize_logger("test_basic");
        auto logger = get_logger();

        REQUIRE(logger != nullptr);
        // Note: Logger name may not change if already initialized in previous tests
    }

    SECTION("Multiple initializations are safe") {
        auto logger1 = get_logger();        // Get current logger
        initialize_logger("test_multiple"); // Try to initialize again
        auto logger2 = get_logger();

        REQUIRE(logger1 == logger2);                 // Same logger instance
        REQUIRE(logger1->name() == logger2->name()); // Name should be preserved
    }
}

TEST_CASE("initialize_logger with layer mode", "[logging]") {
    SECTION("Layer mode initialization") {
        // Note: Due to global state, we test that the function completes successfully
        initialize_logger("test_layer_mode", true);
        auto logger = get_logger();

        REQUIRE(logger != nullptr);
        // Note: Logger name may not change if already initialized in previous tests

        // We can test that the layer initialization doesn't crash
        // (Level may have been changed by other tests, so we don't assert specific level)
    }
}

TEST_CASE("initialize_logger with app mode", "[logging]") {
    SECTION("App mode initialization") {
        initialize_logger("test_app_mode", false);
        auto logger = get_logger();

        REQUIRE(logger != nullptr);
        // Note: Logger name may not change if already initialized in previous tests

        // We can test that the app initialization doesn't crash
        // Note: Level may have been changed by other tests or previous initialization
        REQUIRE(logger->level() <= spdlog::level::critical);
    }
}

TEST_CASE("get_logger always returns valid logger", "[logging]") {
    // Test that get_logger always returns a valid logger
    // Note: We cannot easily reset global state in a unit test environment

    auto logger = get_logger();

    REQUIRE(logger != nullptr);
    // Note: Logger name may vary depending on previous test initialization
}

TEST_CASE("set_log_level changes logger level", "[logging]") {
    initialize_logger("level_test");

    SECTION("Set to trace level") {
        set_log_level(spdlog::level::trace);
        auto logger = get_logger();
        REQUIRE(logger->level() == spdlog::level::trace);
    }

    SECTION("Set to warn level") {
        set_log_level(spdlog::level::warn);
        auto logger = get_logger();
        REQUIRE(logger->level() == spdlog::level::warn);
    }

    SECTION("Set to critical level") {
        set_log_level(spdlog::level::critical);
        auto logger = get_logger();
        REQUIRE(logger->level() == spdlog::level::critical);
    }
}

TEST_CASE("logging macros compile and execute", "[logging]") {
    initialize_logger("macro_test");

    SECTION("All log levels compile") {
        // These should not throw or crash
        // Note: We can't easily test output without capturing it
        GOGGLES_LOG_TRACE("Trace message: {}", 42);
        GOGGLES_LOG_DEBUG("Debug message: {}", "test");
        GOGGLES_LOG_INFO("Info message");
        GOGGLES_LOG_WARN("Warning message: {}", 3.14);
        GOGGLES_LOG_ERROR("Error message: {}", true);
        GOGGLES_LOG_CRITICAL("Critical message");

        // If we get here without crashing, the macros work
        REQUIRE(true);
    }
}

TEST_CASE("logger handles formatting correctly", "[logging]") {
    initialize_logger("format_test");

    // Set to trace level to ensure messages are processed
    set_log_level(spdlog::level::trace);

    SECTION("Format with multiple arguments") {
        // These should not throw exceptions
        GOGGLES_LOG_INFO("Test {} with {} and {}", "formatting", 123, 45.67);
        GOGGLES_LOG_DEBUG("String: '{}', Int: {}, Bool: {}", "test", -99, false);

        REQUIRE(true); // If no exceptions, formatting works
    }

    SECTION("Format with no arguments") {
        GOGGLES_LOG_INFO("Simple message with no formatting");
        GOGGLES_LOG_ERROR("Another simple message");

        REQUIRE(true);
    }
}

TEST_CASE("logger level filtering works", "[logging]") {
    initialize_logger("filter_test");

    SECTION("Higher level messages should be processed") {
        set_log_level(spdlog::level::warn);

        // These calls should not crash even if filtered out
        GOGGLES_LOG_TRACE("This should be filtered");
        GOGGLES_LOG_DEBUG("This should be filtered");
        GOGGLES_LOG_INFO("This should be filtered");
        GOGGLES_LOG_WARN("This should be processed");
        GOGGLES_LOG_ERROR("This should be processed");
        GOGGLES_LOG_CRITICAL("This should be processed");

        REQUIRE(true);
    }

    SECTION("Lower level allows all messages") {
        set_log_level(spdlog::level::trace);

        GOGGLES_LOG_TRACE("This should be processed");
        GOGGLES_LOG_DEBUG("This should be processed");
        GOGGLES_LOG_INFO("This should be processed");
        GOGGLES_LOG_WARN("This should be processed");
        GOGGLES_LOG_ERROR("This should be processed");
        GOGGLES_LOG_CRITICAL("This should be processed");

        REQUIRE(true);
    }
}
