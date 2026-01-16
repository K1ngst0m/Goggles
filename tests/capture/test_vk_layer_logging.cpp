#define GOGGLES_VKLAYER_LOGGING_TESTING

#include "capture/vk_layer/logging.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

struct StderrCapture {
    int saved_stderr = -1;
    int pipe_fds[2] = {-1, -1};

    void start() {
        REQUIRE(pipe(pipe_fds) == 0);

        saved_stderr = dup(STDERR_FILENO);
        REQUIRE(saved_stderr >= 0);

        REQUIRE(dup2(pipe_fds[1], STDERR_FILENO) >= 0);
        close(pipe_fds[1]);
        pipe_fds[1] = -1;
    }

    auto stop() -> std::string {
        if (saved_stderr >= 0) {
            REQUIRE(dup2(saved_stderr, STDERR_FILENO) >= 0);
            close(saved_stderr);
            saved_stderr = -1;
        }

        std::string out;
        std::vector<char> buf(4096);
        for (;;) {
            ssize_t n = read(pipe_fds[0], buf.data(), buf.size());
            if (n == 0) {
                break;
            }
            REQUIRE(n >= 0);
            out.append(buf.data(), static_cast<size_t>(n));
        }
        close(pipe_fds[0]);
        pipe_fds[0] = -1;
        return out;
    }
};

auto count_substr(const std::string& haystack, const std::string& needle) -> size_t {
    size_t count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

} // namespace

TEST_CASE("vk-layer logging is disabled by default", "[capture][vk_layer][logging]") {
    unsetenv("GOGGLES_DEBUG_LOG");
    unsetenv("GOGGLES_DEBUG_LOG_LEVEL");
    goggles::capture::vklayer_logging_detail::test_reset();

    StderrCapture cap;
    cap.start();
    LAYER_ERROR("error %d", 1);
    LAYER_DEBUG("debug %d", 2);
    const std::string out = cap.stop();

    REQUIRE(out.empty());
}

TEST_CASE("vk-layer logging defaults to info when enabled", "[capture][vk_layer][logging]") {
    setenv("GOGGLES_DEBUG_LOG", "1", 1);
    unsetenv("GOGGLES_DEBUG_LOG_LEVEL");
    goggles::capture::vklayer_logging_detail::test_reset();

    StderrCapture cap;
    cap.start();
    LAYER_DEBUG("debug");
    LAYER_WARN("warn");
    LAYER_ERROR("error");
    const std::string out = cap.stop();

    REQUIRE(out.find("[goggles_vklayer]") != std::string::npos);
    REQUIRE(out.find("WARN: warn") != std::string::npos);
    REQUIRE(out.find("ERROR: error") != std::string::npos);
    REQUIRE(out.find("DEBUG: debug") == std::string::npos);
}

TEST_CASE("vk-layer logging respects GOGGLES_DEBUG_LOG_LEVEL", "[capture][vk_layer][logging]") {
    setenv("GOGGLES_DEBUG_LOG", "1", 1);
    setenv("GOGGLES_DEBUG_LOG_LEVEL", "debug", 1);
    goggles::capture::vklayer_logging_detail::test_reset();

    StderrCapture cap;
    cap.start();
    LAYER_DEBUG("debug %d", 1);
    const std::string out = cap.stop();

    REQUIRE(out.find("DEBUG: debug 1") != std::string::npos);
}

TEST_CASE("vk-layer logging invalid level falls back to info", "[capture][vk_layer][logging]") {
    setenv("GOGGLES_DEBUG_LOG", "1", 1);
    setenv("GOGGLES_DEBUG_LOG_LEVEL", "not-a-level", 1);
    goggles::capture::vklayer_logging_detail::test_reset();

    StderrCapture cap;
    cap.start();
    LAYER_DEBUG("debug");
    LAYER_WARN("warn");
    const std::string out = cap.stop();

    REQUIRE(out.find("WARN: warn") != std::string::npos);
    REQUIRE(out.find("DEBUG: debug") == std::string::npos);
}

TEST_CASE("vk-layer logging anti-spam helpers", "[capture][vk_layer][logging]") {
    setenv("GOGGLES_DEBUG_LOG", "1", 1);
    setenv("GOGGLES_DEBUG_LOG_LEVEL", "warn", 1);
    goggles::capture::vklayer_logging_detail::test_reset();

    StderrCapture cap;
    cap.start();
    for (int i = 0; i < 2; ++i) {
        LAYER_WARN_ONCE("once");
    }

    for (int i = 0; i < 7; ++i) {
        LAYER_WARN_EVERY_N(3, "every");
    }

    const std::string out = cap.stop();
    REQUIRE(count_substr(out, "WARN: once") == 1);
    REQUIRE(count_substr(out, "WARN: every") == 2);
}

TEST_CASE("vk-layer logging truncation still ends with newline", "[capture][vk_layer][logging]") {
    setenv("GOGGLES_DEBUG_LOG", "1", 1);
    setenv("GOGGLES_DEBUG_LOG_LEVEL", "info", 1);
    goggles::capture::vklayer_logging_detail::test_reset();

    std::string big(5000, 'a');

    StderrCapture cap;
    cap.start();
    LAYER_WARN("%s", big.c_str());
    const std::string out = cap.stop();

    REQUIRE(!out.empty());
    REQUIRE(out.front() == '[');
    REQUIRE(out.back() == '\n');
}
