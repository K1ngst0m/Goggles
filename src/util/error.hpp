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
    invalid_data,
    unknown_error
};
;

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
using ResultPtr = Result<std::unique_ptr<T>>;

template <typename T>
[[nodiscard]] inline auto make_error(ErrorCode code, std::string message,
                                     std::source_location loc = std::source_location::current())
    -> Result<T> {
    return nonstd::make_unexpected(Error{code, std::move(message), loc});
}

template <typename T>
[[nodiscard]] inline auto make_result_ptr(std::unique_ptr<T> ptr) -> ResultPtr<T> {
    return ResultPtr<T>{std::move(ptr)};
}

template <typename T>
[[nodiscard]] inline auto
make_result_ptr_error(ErrorCode code, std::string message,
                      std::source_location loc = std::source_location::current()) -> ResultPtr<T> {
    return nonstd::make_unexpected(Error{code, std::move(message), loc});
}

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
    case ErrorCode::invalid_data:
        return "invalid_data";
    case ErrorCode::unknown_error:
        return "unknown_error";
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
