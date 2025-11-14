#include <catch2/catch_test_macros.hpp>

#include "util/error.hpp"

#include <string>

using namespace goggles;

TEST_CASE("ErrorCode enum values are correct", "[error]") {
    REQUIRE(static_cast<int>(ErrorCode::ok) == 0);
    REQUIRE(ErrorCode::file_not_found != ErrorCode::ok);
    REQUIRE(ErrorCode::vulkan_init_failed != ErrorCode::parse_error);
}

TEST_CASE("error_code_name returns correct strings", "[error]") {
    REQUIRE(std::string(error_code_name(ErrorCode::ok)) == "ok");
    REQUIRE(std::string(error_code_name(ErrorCode::file_not_found)) == "file_not_found");
    REQUIRE(std::string(error_code_name(ErrorCode::vulkan_init_failed)) == "vulkan_init_failed");
    REQUIRE(std::string(error_code_name(ErrorCode::shader_compile_failed)) == "shader_compile_failed");
    REQUIRE(std::string(error_code_name(ErrorCode::unknown_error)) == "unknown_error");
}

TEST_CASE("Error struct construction", "[error]") {
    SECTION("Basic construction") {
        Error error{ErrorCode::file_not_found, "Test message"};
        REQUIRE(error.code == ErrorCode::file_not_found);
        REQUIRE(error.message == "Test message");
        REQUIRE(error.location.file_name() != nullptr); // source_location should be populated
    }

    SECTION("Construction with custom source location") {
        auto loc = std::source_location::current();
        Error error{ErrorCode::parse_error, "Parse failed", loc};
        REQUIRE(error.code == ErrorCode::parse_error);
        REQUIRE(error.message == "Parse failed");
        REQUIRE(error.location.line() == loc.line());
    }
}

TEST_CASE("Result<T> success cases", "[error]") {
    SECTION("Success with value") {
        auto result = Result<int>{42};
        REQUIRE(result.has_value());
        REQUIRE(result.value() == 42);
        REQUIRE(*result == 42);
    }

    SECTION("Success with string") {
        auto result = Result<std::string>{"success"};
        REQUIRE(result.has_value());
        REQUIRE(result.value() == "success");
    }

    SECTION("Boolean conversion for success") {
        auto result = Result<int>{100};
        REQUIRE(static_cast<bool>(result));
        if (result) {
            REQUIRE(result.value() == 100);
        }
    }
}

TEST_CASE("Result<T> error cases", "[error]") {
    SECTION("Error result") {
        auto result = make_error<int>(ErrorCode::file_not_found, "File missing");
        REQUIRE(!result.has_value());
        REQUIRE(result.error().code == ErrorCode::file_not_found);
        REQUIRE(result.error().message == "File missing");
    }

    SECTION("Boolean conversion for error") {
        auto result = make_error<std::string>(ErrorCode::parse_error, "Invalid syntax");
        REQUIRE(!static_cast<bool>(result));
        if (!result) {
            REQUIRE(result.error().code == ErrorCode::parse_error);
            REQUIRE(result.error().message == "Invalid syntax");
        }
    }
}

TEST_CASE("make_error helper function", "[error]") {
    SECTION("Creates error Result correctly") {
        auto error_result = make_error<double>(ErrorCode::vulkan_device_lost, "Device lost");
        
        REQUIRE(!error_result.has_value());
        REQUIRE(error_result.error().code == ErrorCode::vulkan_device_lost);
        REQUIRE(error_result.error().message == "Device lost");
        REQUIRE(error_result.error().location.file_name() != nullptr);
    }

    SECTION("Source location is captured") {
        auto line_before = static_cast<uint32_t>(__LINE__);
        auto error_result = make_error<int>(ErrorCode::unknown_error, "Test");
        auto line_after = static_cast<uint32_t>(__LINE__);
        
        REQUIRE(error_result.error().location.line() > line_before);
        REQUIRE(error_result.error().location.line() < line_after);
    }
}

TEST_CASE("Result<T> chaining operations", "[error]") {
    SECTION("Transform success case") {
        auto result = Result<int>{10};
        auto transformed = result.transform([](int value) { return value * 2; });
        
        REQUIRE(transformed.has_value());
        REQUIRE(transformed.value() == 20);
    }

    SECTION("Transform error case") {
        auto result = make_error<int>(ErrorCode::file_not_found, "Missing");
        auto transformed = result.transform([](int value) { return value * 2; });
        
        REQUIRE(!transformed.has_value());
        REQUIRE(transformed.error().code == ErrorCode::file_not_found);
        REQUIRE(transformed.error().message == "Missing");
    }

    SECTION("and_then success case") {
        auto result = Result<int>{5};
        auto chained = result.and_then([](int value) -> Result<std::string> {
            return Result<std::string>{std::to_string(value)};
        });
        
        REQUIRE(chained.has_value());
        REQUIRE(chained.value() == "5");
    }

    SECTION("and_then error propagation") {
        auto result = make_error<int>(ErrorCode::parse_error, "Bad input");
        auto chained = result.and_then([](int value) -> Result<std::string> {
            return Result<std::string>{std::to_string(value)};
        });
        
        REQUIRE(!chained.has_value());
        REQUIRE(chained.error().code == ErrorCode::parse_error);
        REQUIRE(chained.error().message == "Bad input");
    }
}
