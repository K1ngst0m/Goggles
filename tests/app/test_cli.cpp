#include "app/cli.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace goggles;

namespace {

struct ArgvBuilder {
    std::vector<std::string> storage;
    std::vector<char*> argv;

    explicit ArgvBuilder(std::initializer_list<std::string> args) : storage(args) {
        argv.reserve(storage.size());
        for (auto& arg : storage) {
            argv.push_back(arg.data());
        }
    }

    [[nodiscard]] auto argc() const -> int { return static_cast<int>(argv.size()); }
};

[[nodiscard]] auto default_config_path() -> std::string {
    return std::string(GOGGLES_SOURCE_DIR) + "/tests/util/test_data/valid_config.toml";
}

} // namespace

TEST_CASE("parse_cli: detach mode accepts no app command", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args({"goggles", "--config", cfg, "--detach"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(result);
    REQUIRE(result->action == goggles::app::CliAction::run);
    REQUIRE(result->options.detach);
    REQUIRE(result->options.app_command.empty());
}

TEST_CASE("parse_cli: detach mode rejects app width/height", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args(
        {"goggles", "--config", cfg, "--detach", "--app-width", "640", "--app-height", "480"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::parse_error);
}

TEST_CASE("parse_cli: detach mode rejects dump options", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args({"goggles", "--config", cfg, "--detach", "--dump-frame-range", "3"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::parse_error);
}

TEST_CASE("parse_cli: detach mode rejects vk-layer logging options", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args({"goggles", "--config", cfg, "--detach", "--layer-log"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::parse_error);
}

TEST_CASE("parse_cli: default mode requires app command after --", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args({"goggles", "--config", cfg});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::parse_error);
}

TEST_CASE("parse_cli: default mode rejects missing -- separator", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args({"goggles", "--config", cfg, "vkcube"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::parse_error);
}

TEST_CASE("parse_cli: default mode parses app command and args", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args({"goggles", "--config", cfg, "--", "vkcube", "--wsi", "xcb"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(result);
    REQUIRE(result->action == goggles::app::CliAction::run);
    REQUIRE(!result->options.detach);
    REQUIRE(result->options.app_command.size() == 3);
    REQUIRE(result->options.app_command[0] == "vkcube");
    REQUIRE(result->options.app_command[1] == "--wsi");
    REQUIRE(result->options.app_command[2] == "xcb");
}

TEST_CASE("parse_cli: default mode parses dump options", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args({"goggles", "--config", cfg, "--dump-dir", "/tmp/goggles_dump",
                      "--dump-frame-range", "3,5,8-13", "--dump-frame-mode", "ppm", "--",
                      "vkcube"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(result);
    REQUIRE(result->action == goggles::app::CliAction::run);
    REQUIRE(!result->options.detach);
    REQUIRE(result->options.dump_dir == "/tmp/goggles_dump");
    REQUIRE(result->options.dump_frame_range == "3,5,8-13");
    REQUIRE(result->options.dump_frame_mode == "ppm");
}

TEST_CASE("parse_cli: default mode parses vk-layer logging options", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args({"goggles", "--config", cfg, "--layer-log-level", "debug", "--", "vkcube"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(result);
    REQUIRE(result->action == goggles::app::CliAction::run);
    REQUIRE(result->options.layer_log);
    REQUIRE(result->options.layer_log_level == "debug");
}

TEST_CASE("parse_cli: app args may include options that collide with viewer flags", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args({"goggles", "--config", cfg, "--", "some_app", "--config", "app.toml"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(result);
    REQUIRE(result->action == goggles::app::CliAction::run);
    REQUIRE(result->options.app_command.size() == 3);
    REQUIRE(result->options.app_command[1] == "--config");
}

TEST_CASE("parse_cli: single-dimension app width/height is allowed", "[cli]") {
    auto cfg = default_config_path();

    SECTION("width only") {
        ArgvBuilder args({"goggles", "--config", cfg, "--app-width", "640", "--", "vkcube"});
        auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
        REQUIRE(result);
        REQUIRE(result->options.app_width == 640);
        REQUIRE(result->options.app_height == 0);
    }

    SECTION("height only") {
        ArgvBuilder args({"goggles", "--config", cfg, "--app-height", "480", "--", "vkcube"});
        auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
        REQUIRE(result);
        REQUIRE(result->options.app_width == 0);
        REQUIRE(result->options.app_height == 480);
    }
}

TEST_CASE("parse_cli: --help returns exit_ok", "[cli]") {
    ArgvBuilder args({"goggles", "--help"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(result);
    REQUIRE(result->action == goggles::app::CliAction::exit_ok);
}

TEST_CASE("parse_cli: --version returns exit_ok", "[cli]") {
    ArgvBuilder args({"goggles", "--version"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(result);
    REQUIRE(result->action == goggles::app::CliAction::exit_ok);
}
