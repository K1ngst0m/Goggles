#include "chain/chain_resources.hpp"
#include "chain/semantic_binder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

using namespace goggles::render;

namespace {

auto parse_original_history_index(std::string_view name) -> std::optional<uint32_t> {
    constexpr std::string_view PREFIX = "OriginalHistory";
    if (!name.starts_with(PREFIX)) {
        return std::nullopt;
    }
    auto suffix = name.substr(PREFIX.size());
    if (suffix.empty()) {
        return std::nullopt;
    }
    uint32_t index = 0;
    auto [ptr, ec] = std::from_chars(suffix.data(), suffix.data() + suffix.size(), index);
    if (ec != std::errc{} || ptr != suffix.data() + suffix.size()) {
        return std::nullopt;
    }
    return index;
}

auto parse_pass_output_index(std::string_view name) -> std::optional<uint32_t> {
    constexpr std::string_view PREFIX = "PassOutput";
    if (!name.starts_with(PREFIX)) {
        return std::nullopt;
    }
    auto suffix = name.substr(PREFIX.size());
    if (suffix.empty()) {
        return std::nullopt;
    }
    uint32_t index = 0;
    auto [ptr, ec] = std::from_chars(suffix.data(), suffix.data() + suffix.size(), index);
    if (ec != std::errc{} || ptr != suffix.data() + suffix.size()) {
        return std::nullopt;
    }
    return index;
}

auto parse_pass_feedback_index(std::string_view name) -> std::optional<uint32_t> {
    constexpr std::string_view PREFIX = "PassFeedback";
    if (!name.starts_with(PREFIX)) {
        return std::nullopt;
    }
    auto suffix = name.substr(PREFIX.size());
    if (suffix.empty()) {
        return std::nullopt;
    }
    uint32_t index = 0;
    auto [ptr, ec] = std::from_chars(suffix.data(), suffix.data() + suffix.size(), index);
    if (ec != std::errc{} || ptr != suffix.data() + suffix.size()) {
        return std::nullopt;
    }
    return index;
}

auto parse_feedback_alias(std::string_view name) -> std::optional<std::string_view> {
    constexpr std::string_view SUFFIX = "Feedback";
    if (!name.ends_with(SUFFIX) || name.size() <= SUFFIX.size()) {
        return std::nullopt;
    }
    return name.substr(0, name.size() - SUFFIX.size());
}

auto contract_root() -> std::filesystem::path {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

auto read_text_file(const std::filesystem::path& path) -> std::optional<std::string> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
} // namespace

TEST_CASE("FilterChain stage ordering parity", "[filter_chain][pipeline]") {
    const auto source_path = contract_root() / "src/chain/chain_executor.cpp";
    auto source_text = read_text_file(source_path);
    REQUIRE(source_text.has_value());

    const auto prechain_pos = source_text->find("record_prechain(resources, cmd");
    const auto effect_pos =
        source_text->find("for (size_t i = 0; i < resources.m_passes.size(); ++i)", prechain_pos);
    auto postchain_pos = source_text->find("record_final_composition(resources, cmd", effect_pos);
    if (postchain_pos == std::string::npos) {
        postchain_pos = source_text->find("record_postchain(resources, cmd", effect_pos);
    }

    REQUIRE(prechain_pos != std::string::npos);
    REQUIRE(effect_pos != std::string::npos);
    REQUIRE(postchain_pos != std::string::npos);
    REQUIRE(prechain_pos < effect_pos);
    REQUIRE(effect_pos < postchain_pos);
}

TEST_CASE("Feedback layout continuity safeguards", "[filter_chain][feedback]") {
    const auto source_path = contract_root() / "src/chain/chain_executor.cpp";
    auto source_text = read_text_file(source_path);
    REQUIRE(source_text.has_value());

    REQUIRE(source_text->find("pre[1].oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;") !=
            std::string::npos);
    REQUIRE(source_text->find("resources.m_feedback_initialized[pass_idx] = true;") !=
            std::string::npos);
}

TEST_CASE("FilterChain image-backed resource teardown destroys images before memory",
          "[filter_chain][lifetime]") {
    const auto framebuffer_path = contract_root() / "src/chain/framebuffer.cpp";
    const auto resources_path = contract_root() / "src/chain/chain_resources.cpp";

    auto framebuffer_text = read_text_file(framebuffer_path);
    auto resources_text = read_text_file(resources_path);
    REQUIRE(framebuffer_text.has_value());
    REQUIRE(resources_text.has_value());

    const auto resize_pos = framebuffer_text->find("auto Framebuffer::resize(");
    const auto shutdown_pos = framebuffer_text->find("void Framebuffer::shutdown()");
    REQUIRE(resize_pos != std::string::npos);
    REQUIRE(shutdown_pos != std::string::npos);

    // Bound resize search to the resize function body (before shutdown).
    const auto resize_destroy_pos =
        framebuffer_text->find("m_device.destroyImage(m_image);", resize_pos);
    const auto resize_free_pos =
        framebuffer_text->find("m_device.freeMemory(m_memory);", resize_pos);
    REQUIRE(resize_destroy_pos != std::string::npos);
    REQUIRE(resize_destroy_pos < shutdown_pos);
    REQUIRE(resize_free_pos != std::string::npos);
    REQUIRE(resize_free_pos < shutdown_pos);
    REQUIRE(resize_destroy_pos < resize_free_pos);

    // Bound shutdown search to the shutdown function body (before next function).
    const auto after_shutdown_pos =
        framebuffer_text->find("auto Framebuffer::create_image()", shutdown_pos);
    REQUIRE(after_shutdown_pos != std::string::npos);

    const auto shutdown_destroy_pos =
        framebuffer_text->find("m_device.destroyImage(m_image);", shutdown_pos);
    const auto shutdown_free_pos =
        framebuffer_text->find("m_device.freeMemory(m_memory);", shutdown_pos);
    REQUIRE(shutdown_destroy_pos != std::string::npos);
    REQUIRE(shutdown_destroy_pos < after_shutdown_pos);
    REQUIRE(shutdown_free_pos != std::string::npos);
    REQUIRE(shutdown_free_pos < after_shutdown_pos);
    REQUIRE(shutdown_destroy_pos < shutdown_free_pos);

    const auto cleanup_pos =
        resources_text->find("void ChainResources::cleanup_texture_registry()");
    REQUIRE(cleanup_pos != std::string::npos);

    // Bound cleanup search to the cleanup function body (before next function).
    const auto after_cleanup_pos =
        resources_text->find("void ChainResources::set_prechain_resolution(", cleanup_pos);
    REQUIRE(after_cleanup_pos != std::string::npos);

    const auto cleanup_destroy_pos =
        resources_text->find("m_vk_ctx.device.destroyImage(tex.data.image);", cleanup_pos);
    const auto cleanup_free_pos =
        resources_text->find("m_vk_ctx.device.freeMemory(tex.data.memory);", cleanup_pos);
    REQUIRE(cleanup_destroy_pos != std::string::npos);
    REQUIRE(cleanup_destroy_pos < after_cleanup_pos);
    REQUIRE(cleanup_free_pos != std::string::npos);
    REQUIRE(cleanup_free_pos < after_cleanup_pos);
    REQUIRE(cleanup_destroy_pos < cleanup_free_pos);
}

TEST_CASE("OriginalHistory sampler name parsing", "[filter_chain][history]") {
    SECTION("Valid OriginalHistory names") {
        auto idx0 = parse_original_history_index("OriginalHistory0");
        REQUIRE(idx0.has_value());
        REQUIRE(*idx0 == 0);

        auto idx3 = parse_original_history_index("OriginalHistory3");
        REQUIRE(idx3.has_value());
        REQUIRE(*idx3 == 3);

        auto idx6 = parse_original_history_index("OriginalHistory6");
        REQUIRE(idx6.has_value());
        REQUIRE(*idx6 == 6);
    }

    SECTION("Invalid OriginalHistory names") {
        REQUIRE(!parse_original_history_index("OriginalHistory").has_value());
        REQUIRE(!parse_original_history_index("OriginalHistoryX").has_value());
        REQUIRE(!parse_original_history_index("OriginalHistory-1").has_value());
        REQUIRE(!parse_original_history_index("Original").has_value());
        REQUIRE(!parse_original_history_index("Source").has_value());
        REQUIRE(!parse_original_history_index("PassOutput0").has_value());
    }

    SECTION("Multi-digit indices") {
        auto idx10 = parse_original_history_index("OriginalHistory10");
        REQUIRE(idx10.has_value());
        REQUIRE(*idx10 == 10);

        auto idx99 = parse_original_history_index("OriginalHistory99");
        REQUIRE(idx99.has_value());
        REQUIRE(*idx99 == 99);
    }
}

TEST_CASE("FilterChain size calculation", "[filter_chain]") {
    ShaderPassConfig config{};
    vk::Extent2D source{256, 224};
    vk::Extent2D viewport{1920, 1080};

    SECTION("SOURCE scale type multiplies source size") {
        config.scale_type_x = ScaleType::source;
        config.scale_type_y = ScaleType::source;
        config.scale_x = 2.0F;
        config.scale_y = 2.0F;

        auto result = ChainResources::calculate_pass_output_size(config, source, viewport);
        REQUIRE(result.width == 512);
        REQUIRE(result.height == 448);
    }

    SECTION("VIEWPORT scale type multiplies viewport size") {
        config.scale_type_x = ScaleType::viewport;
        config.scale_type_y = ScaleType::viewport;
        config.scale_x = 0.5F;
        config.scale_y = 0.5F;

        auto result = ChainResources::calculate_pass_output_size(config, source, viewport);
        REQUIRE(result.width == 960);
        REQUIRE(result.height == 540);
    }

    SECTION("ABSOLUTE scale type uses pixel dimensions") {
        config.scale_type_x = ScaleType::absolute;
        config.scale_type_y = ScaleType::absolute;
        config.scale_x = 640.0F;
        config.scale_y = 480.0F;

        auto result = ChainResources::calculate_pass_output_size(config, source, viewport);
        REQUIRE(result.width == 640);
        REQUIRE(result.height == 480);
    }

    SECTION("Mixed scale types work independently") {
        config.scale_type_x = ScaleType::source;
        config.scale_type_y = ScaleType::viewport;
        config.scale_x = 4.0F;
        config.scale_y = 1.0F;

        auto result = ChainResources::calculate_pass_output_size(config, source, viewport);
        REQUIRE(result.width == 1024);
        REQUIRE(result.height == 1080);
    }

    SECTION("Minimum size is 1x1") {
        config.scale_type_x = ScaleType::source;
        config.scale_type_y = ScaleType::source;
        config.scale_x = 0.0F;
        config.scale_y = 0.0F;

        auto result = ChainResources::calculate_pass_output_size(config, source, viewport);
        REQUIRE(result.width == 1);
        REQUIRE(result.height == 1);
    }

    SECTION("Fractional scaling rounds correctly") {
        config.scale_type_x = ScaleType::source;
        config.scale_type_y = ScaleType::source;
        config.scale_x = 1.5F;
        config.scale_y = 1.5F;

        auto result = ChainResources::calculate_pass_output_size(config, source, viewport);
        REQUIRE(result.width == 384);
        REQUIRE(result.height == 336);
    }
}

TEST_CASE("PassOutput# sampler name parsing", "[filter_chain][shader_spec]") {
    SECTION("Valid PassOutput names") {
        REQUIRE(parse_pass_output_index("PassOutput0").value() == 0);
        REQUIRE(parse_pass_output_index("PassOutput1").value() == 1);
        REQUIRE(parse_pass_output_index("PassOutput12").value() == 12);
    }

    SECTION("Invalid PassOutput names") {
        REQUIRE(!parse_pass_output_index("PassOutput").has_value());
        REQUIRE(!parse_pass_output_index("PassOutputX").has_value());
        REQUIRE(!parse_pass_output_index("OriginalHistory0").has_value());
        REQUIRE(!parse_pass_output_index("Source").has_value());
    }
}

TEST_CASE("PassFeedback# sampler name parsing", "[filter_chain][shader_spec]") {
    SECTION("Valid PassFeedback names") {
        REQUIRE(parse_pass_feedback_index("PassFeedback0").value() == 0);
        REQUIRE(parse_pass_feedback_index("PassFeedback5").value() == 5);
        REQUIRE(parse_pass_feedback_index("PassFeedback13").value() == 13);
    }

    SECTION("Invalid PassFeedback names") {
        REQUIRE(!parse_pass_feedback_index("PassFeedback").has_value());
        REQUIRE(!parse_pass_feedback_index("PassOutput0").has_value());
        REQUIRE(!parse_pass_feedback_index("DerezedPassFeedback").has_value());
    }
}

TEST_CASE("Semantic binding parity", "[filter_chain][shader_spec]") {
    SECTION("Source semantic remains explicit") {
        const auto source_path = contract_root() / "src/chain/chain_executor.cpp";
        auto source_text = read_text_file(source_path);
        REQUIRE(source_text.has_value());
        REQUIRE(source_text->find("\"Source\"") != std::string::npos);
    }

    SECTION("Alias feedback semantic parsing") {
        REQUIRE(parse_feedback_alias("DerezedPassFeedback").value() == "DerezedPass");
        REQUIRE(parse_feedback_alias("MaskFeedback").value() == "Mask");
    }

    SECTION("Invalid alias feedback names") {
        REQUIRE(!parse_feedback_alias("Feedback").has_value());
        REQUIRE(!parse_feedback_alias("PassOutput0").has_value());
        REQUIRE(!parse_feedback_alias("PassFeedback0").has_value());
    }
}

TEST_CASE("SizeVec4 format per spec", "[filter_chain][shader_spec]") {
    SECTION("make_size_vec4 produces correct vec4") {
        auto size = make_size_vec4(1920, 1080);
        REQUIRE(size.width == 1920.0F);
        REQUIRE(size.height == 1080.0F);
        REQUIRE(size.inv_width == Catch::Approx(1.0F / 1920.0F));
        REQUIRE(size.inv_height == Catch::Approx(1.0F / 1080.0F));
    }

    SECTION("Size vec4 data() returns contiguous floats") {
        auto size = make_size_vec4(256, 224);
        const float* data = size.data();
        REQUIRE(data[0] == 256.0F);
        REQUIRE(data[1] == 224.0F);
        REQUIRE(data[2] == Catch::Approx(1.0F / 256.0F));
        REQUIRE(data[3] == Catch::Approx(1.0F / 224.0F));
    }
}

TEST_CASE("SemanticBinder alias sizes", "[filter_chain][shader_spec]") {
    SemanticBinder binder;

    SECTION("Alias sizes are stored and retrieved") {
        binder.set_alias_size("DerezedPass", 320, 240);
        auto size = binder.get_alias_size("DerezedPass");
        REQUIRE(size.has_value());
        REQUIRE(size->width == 320.0F);
        REQUIRE(size->height == 240.0F);
    }

    SECTION("Unknown alias returns nullopt") {
        REQUIRE(!binder.get_alias_size("NonExistent").has_value());
    }

    SECTION("clear_alias_sizes removes all aliases") {
        binder.set_alias_size("Pass0", 100, 100);
        binder.set_alias_size("Pass1", 200, 200);
        binder.clear_alias_sizes();
        REQUIRE(!binder.get_alias_size("Pass0").has_value());
        REQUIRE(!binder.get_alias_size("Pass1").has_value());
    }
}
