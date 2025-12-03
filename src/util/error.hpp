#pragma once

#include <cstdint>
#include <nonstd/expected.hpp>
#include <source_location>
#include <string>
#include <utility>

namespace goggles {

enum class ErrorCode : std::uint8_t {
    OK,
    FILE_NOT_FOUND,
    FILE_READ_FAILED,
    FILE_WRITE_FAILED,
    PARSE_ERROR,
    INVALID_CONFIG,
    VULKAN_INIT_FAILED,
    VULKAN_DEVICE_LOST,
    SHADER_COMPILE_FAILED,
    SHADER_LOAD_FAILED,
    CAPTURE_INIT_FAILED,
    CAPTURE_FRAME_FAILED,
    UNKNOWN_ERROR
};

struct Error {
    ErrorCode code;
    std::string message;
    std::source_location location;

    Error(ErrorCode error_code, std::string msg,
          std::source_location loc = std::source_location::current())
        : code(error_code), message(std::move(msg)), location(loc) {}
};

template <typename T>
using Result = nonstd::expected<T, Error>;

template <typename T>
[[nodiscard]] inline auto make_error(ErrorCode code, std::string message,
                                     std::source_location loc = std::source_location::current())
    -> Result<T> {
    return nonstd::make_unexpected(Error{code, std::move(message), loc});
}

[[nodiscard]] constexpr auto error_code_name(ErrorCode code) -> const char* {
    switch (code) {
    case ErrorCode::OK:
        return "OK";
    case ErrorCode::FILE_NOT_FOUND:
        return "FILE_NOT_FOUND";
    case ErrorCode::FILE_READ_FAILED:
        return "FILE_READ_FAILED";
    case ErrorCode::FILE_WRITE_FAILED:
        return "FILE_WRITE_FAILED";
    case ErrorCode::PARSE_ERROR:
        return "PARSE_ERROR";
    case ErrorCode::INVALID_CONFIG:
        return "INVALID_CONFIG";
    case ErrorCode::VULKAN_INIT_FAILED:
        return "VULKAN_INIT_FAILED";
    case ErrorCode::VULKAN_DEVICE_LOST:
        return "VULKAN_DEVICE_LOST";
    case ErrorCode::SHADER_COMPILE_FAILED:
        return "SHADER_COMPILE_FAILED";
    case ErrorCode::SHADER_LOAD_FAILED:
        return "SHADER_LOAD_FAILED";
    case ErrorCode::CAPTURE_INIT_FAILED:
        return "CAPTURE_INIT_FAILED";
    case ErrorCode::CAPTURE_FRAME_FAILED:
        return "CAPTURE_FRAME_FAILED";
    case ErrorCode::UNKNOWN_ERROR:
        return "UNKNOWN_ERROR";
    }
    return "unknown";
}

} // namespace goggles
