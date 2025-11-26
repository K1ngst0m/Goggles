#pragma once

#include <cstdint>
#include <nonstd/expected.hpp>
#include <source_location>
#include <string>
#include <utility>

namespace goggles {

// Error codes for categorizing failures
enum class ErrorCode : std::uint8_t {
    ok,
    file_not_found,
    file_read_failed,
    file_write_failed,
    parse_error,
    invalid_config,
    vulkan_init_failed,
    vulkan_device_lost,
    shader_compile_failed,
    shader_load_failed,
    capture_init_failed,
    capture_frame_failed,
    unknown_error
};

// Lightweight error type for use with tl::expected
struct Error {
    ErrorCode code;
    std::string message;
    std::source_location location;

    Error(ErrorCode error_code, std::string msg,
          std::source_location loc = std::source_location::current())
        : code(error_code), message(std::move(msg)), location(loc) {}
};

// Alias for expected type using nonstd::expected (expected-lite)
template <typename T>
using Result = nonstd::expected<T, Error>;

// Helper function to create an error Result
template <typename T>
[[nodiscard]] inline auto make_error(ErrorCode code, std::string message,
                                     std::source_location loc = std::source_location::current())
    -> Result<T> {
    return nonstd::make_unexpected(Error{code, std::move(message), loc});
}

// Helper function to get error code name as string (for logging)
[[nodiscard]] constexpr auto error_code_name(ErrorCode code) -> const char* {
    switch (code) {
    case ErrorCode::ok:
        return "ok";
    case ErrorCode::file_not_found:
        return "file_not_found";
    case ErrorCode::file_read_failed:
        return "file_read_failed";
    case ErrorCode::file_write_failed:
        return "file_write_failed";
    case ErrorCode::parse_error:
        return "parse_error";
    case ErrorCode::invalid_config:
        return "invalid_config";
    case ErrorCode::vulkan_init_failed:
        return "vulkan_init_failed";
    case ErrorCode::vulkan_device_lost:
        return "vulkan_device_lost";
    case ErrorCode::shader_compile_failed:
        return "shader_compile_failed";
    case ErrorCode::shader_load_failed:
        return "shader_load_failed";
    case ErrorCode::capture_init_failed:
        return "capture_init_failed";
    case ErrorCode::capture_frame_failed:
        return "capture_frame_failed";
    case ErrorCode::unknown_error:
        return "unknown_error";
    }
    return "unknown";
}

} // namespace goggles
