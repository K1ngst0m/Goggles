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
    REQUIRE(result->detach);
    REQUIRE(result->app_command.empty());
}

TEST_CASE("parse_cli: detach mode rejects app width/height", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args(
        {"goggles", "--config", cfg, "--detach", "--app-width", "640", "--app-height", "480"});

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
    REQUIRE(!result->detach);
    REQUIRE(result->app_command.size() == 3);
    REQUIRE(result->app_command[0] == "vkcube");
    REQUIRE(result->app_command[1] == "--wsi");
    REQUIRE(result->app_command[2] == "xcb");
}

TEST_CASE("parse_cli: app args may include options that collide with viewer flags", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args({"goggles", "--config", cfg, "--", "some_app", "--config", "app.toml"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(result);
    REQUIRE(result->app_command.size() == 3);
    REQUIRE(result->app_command[1] == "--config");
}

TEST_CASE("parse_cli: app width/height must be provided together", "[cli]") {
    auto cfg = default_config_path();
    ArgvBuilder args({"goggles", "--config", cfg, "--app-width", "640", "--", "vkcube"});

    auto result = goggles::app::parse_cli(args.argc(), args.argv.data());
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::parse_error);
}
