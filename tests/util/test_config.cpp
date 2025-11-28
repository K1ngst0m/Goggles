#include "util/config.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace goggles;

TEST_CASE("default_config returns expected values", "[config]") {
    auto config = default_config();

    SECTION("Capture defaults") {
        REQUIRE(config.capture.backend == "vulkan_layer");
    }

    SECTION("Pipeline defaults") {
        REQUIRE(config.pipeline.shader_preset.empty());
    }

    SECTION("Render defaults") {
        REQUIRE(config.render.vsync == true);
        REQUIRE(config.render.target_fps == 60);
    }

    SECTION("Logging defaults") {
        REQUIRE(config.logging.level == "info");
        REQUIRE(config.logging.file.empty());
    }
}

TEST_CASE("load_config handles missing file", "[config]") {
    const std::string nonexistent_file = "util/test_data/nonexistent.toml";
    auto result = load_config(nonexistent_file);

    REQUIRE(!result.has_value());
    REQUIRE(result.error().code == ErrorCode::FILE_NOT_FOUND);
    REQUIRE(result.error().message.find("Configuration file not found") != std::string::npos);
    REQUIRE(result.error().message.find(nonexistent_file) != std::string::npos);
}

TEST_CASE("load_config parses valid configuration", "[config]") {
    auto result = load_config("util/test_data/valid_config.toml");

    REQUIRE(result.has_value());
    auto config = result.value();

    SECTION("Capture section") {
        REQUIRE(config.capture.backend == "vulkan_layer");
    }

    SECTION("Pipeline section") {
        REQUIRE(config.pipeline.shader_preset == "shaders/test.glsl");
    }

    SECTION("Render section") {
        REQUIRE(config.render.vsync == false);
        REQUIRE(config.render.target_fps == 120);
    }

    SECTION("Logging section") {
        REQUIRE(config.logging.level == "debug");
        REQUIRE(config.logging.file == "test.log");
    }
}

TEST_CASE("load_config uses defaults for partial configuration", "[config]") {
    auto result = load_config("util/test_data/partial_config.toml");

    REQUIRE(result.has_value());
    auto config = result.value();

    SECTION("Uses defaults for missing sections") {
        REQUIRE(config.capture.backend == "vulkan_layer"); // default
        REQUIRE(config.pipeline.shader_preset.empty());    // default
        REQUIRE(config.logging.level == "info");           // default
        REQUIRE(config.logging.file.empty());              // default
    }

    SECTION("Uses provided values") {
        REQUIRE(config.render.vsync == true); // from file
    }
}

TEST_CASE("load_config validates backend values", "[config]") {
    auto result = load_config("util/test_data/invalid_config.toml");

    REQUIRE(!result.has_value());
    REQUIRE(result.error().code == ErrorCode::INVALID_CONFIG);
    REQUIRE(result.error().message.find("Invalid capture backend") != std::string::npos);
    REQUIRE(result.error().message.find("invalid_backend") != std::string::npos);
    REQUIRE(result.error().message.find("vulkan_layer or compositor") != std::string::npos);
}

TEST_CASE("load_config validates target_fps values", "[config]") {
    // Create a temporary config with invalid FPS
    const std::string temp_config = "util/test_data/invalid_fps.toml";
    std::ofstream file(temp_config);
    file << "[render]\ntarget_fps = -10\n";
    file.close();

    auto result = load_config(temp_config);

    REQUIRE(!result.has_value());
    REQUIRE(result.error().code == ErrorCode::INVALID_CONFIG);
    REQUIRE(result.error().message.find("Invalid target_fps") != std::string::npos);
    REQUIRE(result.error().message.find("-10") != std::string::npos);
    REQUIRE(result.error().message.find("1-1000") != std::string::npos);

    // Clean up
    std::filesystem::remove(temp_config);
}

TEST_CASE("load_config validates target_fps upper bound", "[config]") {
    // Create a temporary config with too high FPS
    const std::string temp_config = "util/test_data/high_fps.toml";
    std::ofstream file(temp_config);
    file << "[render]\ntarget_fps = 2000\n";
    file.close();

    auto result = load_config(temp_config);

    REQUIRE(!result.has_value());
    REQUIRE(result.error().code == ErrorCode::INVALID_CONFIG);
    REQUIRE(result.error().message.find("Invalid target_fps") != std::string::npos);
    REQUIRE(result.error().message.find("2000") != std::string::npos);

    // Clean up
    std::filesystem::remove(temp_config);
}

TEST_CASE("load_config validates log level values", "[config]") {
    // Create temporary config with invalid log level
    const std::string temp_config = "util/test_data/invalid_log_level.toml";
    std::ofstream file(temp_config);
    file << "[logging]\nlevel = \"invalid_level\"\n";
    file.close();

    auto result = load_config(temp_config);

    REQUIRE(!result.has_value());
    REQUIRE(result.error().code == ErrorCode::INVALID_CONFIG);
    REQUIRE(result.error().message.find("Invalid log level") != std::string::npos);
    REQUIRE(result.error().message.find("invalid_level") != std::string::npos);
    REQUIRE(result.error().message.find("trace, debug, info, warn, error, critical") !=
            std::string::npos);

    // Clean up
    std::filesystem::remove(temp_config);
}

TEST_CASE("load_config accepts all valid log levels", "[config]") {
    const std::vector<std::string> valid_levels = {"trace", "debug", "info",
                                                   "warn",  "error", "critical"};

    for (const auto& level : valid_levels) {
        const std::string temp_config = "util/test_data/level_" + level + ".toml";
        std::ofstream file(temp_config);
        file << "[logging]\nlevel = \"" << level << "\"\n";
        file.close();

        auto result = load_config(temp_config);

        REQUIRE(result.has_value());
        REQUIRE(result.value().logging.level == level);

        // Clean up
        std::filesystem::remove(temp_config);
    }
}

TEST_CASE("load_config accepts both valid backends", "[config]") {
    const std::vector<std::string> valid_backends = {"vulkan_layer", "compositor"};

    for (const auto& backend : valid_backends) {
        const std::string temp_config = "util/test_data/backend_" + backend + ".toml";
        std::ofstream file(temp_config);
        file << "[capture]\nbackend = \"" << backend << "\"\n";
        file.close();

        auto result = load_config(temp_config);

        REQUIRE(result.has_value());
        REQUIRE(result.value().capture.backend == backend);

        // Clean up
        std::filesystem::remove(temp_config);
    }
}

TEST_CASE("load_config handles TOML parse errors", "[config]") {
    auto result = load_config("util/test_data/malformed_config.toml");

    REQUIRE(!result.has_value());
    REQUIRE(result.error().code == ErrorCode::PARSE_ERROR);
    REQUIRE(result.error().message.find("Failed to parse TOML") != std::string::npos);
}

TEST_CASE("load_config handles valid target_fps range", "[config]") {
    const std::vector<int> valid_fps_values = {1, 30, 60, 144, 240, 1000};

    for (int fps : valid_fps_values) {
        const std::string temp_config = "util/test_data/fps_" + std::to_string(fps) + ".toml";
        std::ofstream file(temp_config);
        file << "[render]\ntarget_fps = " << fps << "\n";
        file.close();

        auto result = load_config(temp_config);

        REQUIRE(result.has_value());
        REQUIRE(result.value().render.target_fps == static_cast<uint32_t>(fps));

        // Clean up
        std::filesystem::remove(temp_config);
    }
}
