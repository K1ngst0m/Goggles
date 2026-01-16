#pragma once

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <unistd.h>

namespace goggles::capture {

enum class VklayerLogLevel : uint8_t {
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    error = 4,
    critical = 5,
    off = 6,
};

struct VklayerLogConfig {
    bool enabled = false;
    VklayerLogLevel min_level = VklayerLogLevel::info;
};

namespace vklayer_logging_detail {

inline auto to_level_string(VklayerLogLevel level) -> const char* {
    switch (level) {
    case VklayerLogLevel::trace:
        return "TRACE";
    case VklayerLogLevel::debug:
        return "DEBUG";
    case VklayerLogLevel::info:
        return "INFO";
    case VklayerLogLevel::warn:
        return "WARN";
    case VklayerLogLevel::error:
        return "ERROR";
    case VklayerLogLevel::critical:
        return "CRITICAL";
    case VklayerLogLevel::off:
        return "OFF";
    }
    return "UNKNOWN";
}

inline auto equals_ignore_case(std::string_view a, std::string_view b) -> bool {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

inline auto parse_level(const char* env) -> VklayerLogLevel {
    if (env == nullptr || env[0] == '\0') {
        return VklayerLogLevel::info;
    }

    std::string_view s(env);
    if (equals_ignore_case(s, "trace")) {
        return VklayerLogLevel::trace;
    }
    if (equals_ignore_case(s, "debug")) {
        return VklayerLogLevel::debug;
    }
    if (equals_ignore_case(s, "info")) {
        return VklayerLogLevel::info;
    }
    if (equals_ignore_case(s, "warn") || equals_ignore_case(s, "warning")) {
        return VklayerLogLevel::warn;
    }
    if (equals_ignore_case(s, "error")) {
        return VklayerLogLevel::error;
    }
    if (equals_ignore_case(s, "critical") || equals_ignore_case(s, "fatal")) {
        return VklayerLogLevel::critical;
    }
    if (equals_ignore_case(s, "off") || equals_ignore_case(s, "none")) {
        return VklayerLogLevel::off;
    }

    return VklayerLogLevel::info;
}

inline auto init_from_env() -> VklayerLogConfig {
    const char* enabled_env = std::getenv("GOGGLES_DEBUG_LOG");
    const bool enabled =
        enabled_env != nullptr && enabled_env[0] != '\0' && std::strcmp(enabled_env, "0") != 0;

    VklayerLogConfig cfg{};
    cfg.enabled = enabled;
    if (!cfg.enabled) {
        cfg.min_level = VklayerLogLevel::off;
        return cfg;
    }

    cfg.min_level = parse_level(std::getenv("GOGGLES_DEBUG_LOG_LEVEL"));
    if (cfg.min_level == VklayerLogLevel::off) {
        cfg.enabled = false;
    }
    return cfg;
}

inline std::atomic<uint8_t> g_init_state{0};
inline VklayerLogConfig g_config{};

inline auto get_config() -> const VklayerLogConfig& {
    uint8_t state = g_init_state.load(std::memory_order_acquire);
    if (state == 2) {
        return g_config;
    }

    uint8_t expected = 0;
    if (state == 0 &&
        g_init_state.compare_exchange_strong(expected, 1, std::memory_order_acq_rel)) {
        g_config = init_from_env();
        g_init_state.store(2, std::memory_order_release);
        return g_config;
    }

    while (g_init_state.load(std::memory_order_acquire) != 2) {
    }
    return g_config;
}

#if defined(GOGGLES_VKLAYER_LOGGING_TESTING)
inline void test_reset() {
    g_init_state.store(0, std::memory_order_release);
}

inline void test_set_config(bool enabled, VklayerLogLevel min_level) {
    g_init_state.store(1, std::memory_order_release);
    g_config.enabled = enabled;
    g_config.min_level = min_level;
    g_init_state.store(2, std::memory_order_release);
}
#endif

inline auto should_log(VklayerLogLevel level) -> bool {
    const auto& cfg = get_config();
    if (!cfg.enabled) {
        return false;
    }
    return static_cast<uint8_t>(level) >= static_cast<uint8_t>(cfg.min_level);
}

#if defined(__clang__) || defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
inline void write_log(VklayerLogLevel level, const char* fmt, ...) {
    constexpr size_t BUF_SIZE = 1024;
    char buf[BUF_SIZE];

    int prefix_len =
        std::snprintf(buf, sizeof(buf), "[goggles_vklayer] %s: ", to_level_string(level));
    if (prefix_len < 0) {
        return;
    }

    size_t used = static_cast<size_t>(prefix_len);
    if (used >= sizeof(buf)) {
        used = sizeof(buf) - 1;
    }

    std::va_list args;
    va_start(args, fmt);
    int msg_len = std::vsnprintf(buf + used, sizeof(buf) - used, fmt, args);
    va_end(args);

    size_t total = used;
    if (msg_len > 0) {
        total = used + static_cast<size_t>(msg_len);
        if (total >= sizeof(buf)) {
            total = sizeof(buf) - 1;
        }
    }

    if (total == 0 || buf[total - 1] != '\n') {
        if (total < sizeof(buf) - 1) {
            buf[total++] = '\n';
        } else {
            buf[sizeof(buf) - 2] = '\n';
            total = sizeof(buf) - 1;
        }
    }

    (void)::write(STDERR_FILENO, buf, total);
}

} // namespace vklayer_logging_detail

} // namespace goggles::capture

#define LAYER_DEBUG(fmt, ...)                                                                      \
    do {                                                                                           \
        if (::goggles::capture::vklayer_logging_detail::should_log(                                \
                ::goggles::capture::VklayerLogLevel::debug)) {                                     \
            ::goggles::capture::vklayer_logging_detail::write_log(                                 \
                ::goggles::capture::VklayerLogLevel::debug, fmt, ##__VA_ARGS__);                   \
        }                                                                                          \
    } while (0)

#define LAYER_WARN(fmt, ...)                                                                       \
    do {                                                                                           \
        if (::goggles::capture::vklayer_logging_detail::should_log(                                \
                ::goggles::capture::VklayerLogLevel::warn)) {                                      \
            ::goggles::capture::vklayer_logging_detail::write_log(                                 \
                ::goggles::capture::VklayerLogLevel::warn, fmt, ##__VA_ARGS__);                    \
        }                                                                                          \
    } while (0)

#define LAYER_ERROR(fmt, ...)                                                                      \
    do {                                                                                           \
        if (::goggles::capture::vklayer_logging_detail::should_log(                                \
                ::goggles::capture::VklayerLogLevel::error)) {                                     \
            ::goggles::capture::vklayer_logging_detail::write_log(                                 \
                ::goggles::capture::VklayerLogLevel::error, fmt, ##__VA_ARGS__);                   \
        }                                                                                          \
    } while (0)

#define LAYER_CRITICAL(fmt, ...)                                                                   \
    do {                                                                                           \
        if (::goggles::capture::vklayer_logging_detail::should_log(                                \
                ::goggles::capture::VklayerLogLevel::critical)) {                                  \
            ::goggles::capture::vklayer_logging_detail::write_log(                                 \
                ::goggles::capture::VklayerLogLevel::critical, fmt, ##__VA_ARGS__);                \
        }                                                                                          \
    } while (0)

#define LAYER_WARN_ONCE(fmt, ...)                                                                  \
    do {                                                                                           \
        if (::goggles::capture::vklayer_logging_detail::should_log(                                \
                ::goggles::capture::VklayerLogLevel::warn)) {                                      \
            static std::atomic<bool> vklayer_warn_once_emitted{false};                             \
            if (!vklayer_warn_once_emitted.exchange(true, std::memory_order_acq_rel)) {            \
                ::goggles::capture::vklayer_logging_detail::write_log(                             \
                    ::goggles::capture::VklayerLogLevel::warn, fmt, ##__VA_ARGS__);                \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define LAYER_ERROR_ONCE(fmt, ...)                                                                 \
    do {                                                                                           \
        if (::goggles::capture::vklayer_logging_detail::should_log(                                \
                ::goggles::capture::VklayerLogLevel::error)) {                                     \
            static std::atomic<bool> vklayer_error_once_emitted{false};                            \
            if (!vklayer_error_once_emitted.exchange(true, std::memory_order_acq_rel)) {           \
                ::goggles::capture::vklayer_logging_detail::write_log(                             \
                    ::goggles::capture::VklayerLogLevel::error, fmt, ##__VA_ARGS__);               \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define LAYER_WARN_EVERY_N(n, fmt, ...)                                                            \
    do {                                                                                           \
        if (::goggles::capture::vklayer_logging_detail::should_log(                                \
                ::goggles::capture::VklayerLogLevel::warn)) {                                      \
            static std::atomic<uint64_t> vklayer_warn_every_n_counter{0};                          \
            uint64_t vklayer_n = static_cast<uint64_t>(n);                                         \
            if (vklayer_n == 0) {                                                                  \
                vklayer_n = 1;                                                                     \
            }                                                                                      \
            uint64_t vklayer_i =                                                                   \
                vklayer_warn_every_n_counter.fetch_add(1, std::memory_order_relaxed) + 1;          \
            if ((vklayer_i % vklayer_n) == 0) {                                                    \
                ::goggles::capture::vklayer_logging_detail::write_log(                             \
                    ::goggles::capture::VklayerLogLevel::warn, fmt, ##__VA_ARGS__);                \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define LAYER_ERROR_EVERY_N(n, fmt, ...)                                                           \
    do {                                                                                           \
        if (::goggles::capture::vklayer_logging_detail::should_log(                                \
                ::goggles::capture::VklayerLogLevel::error)) {                                     \
            static std::atomic<uint64_t> vklayer_error_every_n_counter{0};                         \
            uint64_t vklayer_n = static_cast<uint64_t>(n);                                         \
            if (vklayer_n == 0) {                                                                  \
                vklayer_n = 1;                                                                     \
            }                                                                                      \
            uint64_t vklayer_i =                                                                   \
                vklayer_error_every_n_counter.fetch_add(1, std::memory_order_relaxed) + 1;         \
            if ((vklayer_i % vklayer_n) == 0) {                                                    \
                ::goggles::capture::vklayer_logging_detail::write_log(                             \
                    ::goggles::capture::VklayerLogLevel::error, fmt, ##__VA_ARGS__);               \
            }                                                                                      \
        }                                                                                          \
    } while (0)
