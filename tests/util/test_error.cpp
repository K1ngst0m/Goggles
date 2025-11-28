#include "util/error.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace goggles;

TEST_CASE("ErrorCode enum values are correct", "[error]") {
    REQUIRE(static_cast<int>(ErrorCode::OK) == 0);
    REQUIRE(ErrorCode::FILE_NOT_FOUND != ErrorCode::OK);
    REQUIRE(ErrorCode::VULKAN_INIT_FAILED != ErrorCode::PARSE_ERROR);
}

TEST_CASE("error_code_name returns correct strings", "[error]") {
    REQUIRE(std::string(error_code_name(ErrorCode::OK)) == "OK");
    REQUIRE(std::string(error_code_name(ErrorCode::FILE_NOT_FOUND)) == "FILE_NOT_FOUND");
    REQUIRE(std::string(error_code_name(ErrorCode::VULKAN_INIT_FAILED)) == "VULKAN_INIT_FAILED");
    REQUIRE(std::string(error_code_name(ErrorCode::SHADER_COMPILE_FAILED)) ==
            "SHADER_COMPILE_FAILED");
    REQUIRE(std::string(error_code_name(ErrorCode::UNKNOWN_ERROR)) == "UNKNOWN_ERROR");
}

TEST_CASE("Error struct construction", "[error]") {
    SECTION("Basic construction") {
        Error error{ErrorCode::FILE_NOT_FOUND, "Test message"};
        REQUIRE(error.code == ErrorCode::FILE_NOT_FOUND);
        REQUIRE(error.message == "Test message");
        REQUIRE(error.location.file_name() != nullptr); // source_location should be populated
    }

    SECTION("Construction with custom source location") {
        auto loc = std::source_location::current();
        Error error{ErrorCode::PARSE_ERROR, "Parse failed", loc};
        REQUIRE(error.code == ErrorCode::PARSE_ERROR);
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
        auto result = make_error<int>(ErrorCode::FILE_NOT_FOUND, "File missing");
        REQUIRE(!result.has_value());
        REQUIRE(result.error().code == ErrorCode::FILE_NOT_FOUND);
        REQUIRE(result.error().message == "File missing");
    }

    SECTION("Boolean conversion for error") {
        auto result = make_error<std::string>(ErrorCode::PARSE_ERROR, "Invalid syntax");
        REQUIRE(!static_cast<bool>(result));
        if (!result) {
            REQUIRE(result.error().code == ErrorCode::PARSE_ERROR);
            REQUIRE(result.error().message == "Invalid syntax");
        }
    }
}

TEST_CASE("make_error helper function", "[error]") {
    SECTION("Creates error Result correctly") {
        auto error_result = make_error<double>(ErrorCode::VULKAN_DEVICE_LOST, "Device lost");

        REQUIRE(!error_result.has_value());
        REQUIRE(error_result.error().code == ErrorCode::VULKAN_DEVICE_LOST);
        REQUIRE(error_result.error().message == "Device lost");
        REQUIRE(error_result.error().location.file_name() != nullptr);
    }

    SECTION("Source location is captured") {
        auto line_before = static_cast<uint32_t>(__LINE__);
        auto error_result = make_error<int>(ErrorCode::UNKNOWN_ERROR, "Test");
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
        auto result = make_error<int>(ErrorCode::FILE_NOT_FOUND, "Missing");
        auto transformed = result.transform([](int value) { return value * 2; });

        REQUIRE(!transformed.has_value());
        REQUIRE(transformed.error().code == ErrorCode::FILE_NOT_FOUND);
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
        auto result = make_error<int>(ErrorCode::PARSE_ERROR, "Bad input");
        auto chained = result.and_then([](int value) -> Result<std::string> {
            return Result<std::string>{std::to_string(value)};
        });

        REQUIRE(!chained.has_value());
        REQUIRE(chained.error().code == ErrorCode::PARSE_ERROR);
        REQUIRE(chained.error().message == "Bad input");
    }
}
