#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
    INVALID_DATA,
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
    case ErrorCode::INVALID_DATA:
        return "INVALID_DATA";
    case ErrorCode::UNKNOWN_ERROR:
        return "UNKNOWN_ERROR";
    }
    return "unknown";
}

} // namespace goggles

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/// Propagate error or return value. Expression-style like Rust's `?` operator.
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define GOGGLES_TRY(expr)                                                                          \
    ({                                                                                             \
        auto _try_result = (expr);                                                                 \
        if (!_try_result)                                                                          \
            return nonstd::make_unexpected(_try_result.error());                                   \
        std::move(_try_result).value();                                                            \
    })

/// Abort on error or return value. Use for internal invariants where failure is a bug.
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define GOGGLES_MUST(expr)                                                                         \
    ({                                                                                             \
        auto _must_result = (expr);                                                                \
        if (!_must_result) {                                                                       \
            auto& _err = _must_result.error();                                                     \
            std::fprintf(stderr, "GOGGLES_MUST failed at %s:%u in %s\n  %s: %s\n",                 \
                         _err.location.file_name(), _err.location.line(),                          \
                         _err.location.function_name(), goggles::error_code_name(_err.code),       \
                         _err.message.c_str());                                                    \
            std::abort();                                                                          \
        }                                                                                          \
        std::move(_must_result).value();                                                           \
    })

// NOLINTEND(cppcoreguidelines-macro-usage)
