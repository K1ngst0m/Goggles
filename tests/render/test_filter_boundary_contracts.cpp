#include "compositor/compositor_runtime_metrics.hpp"
#include "render/backend/vulkan_backend.hpp"
#include "render/chain/filter_controls.hpp"
#include "ui/imgui_layer.hpp"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

namespace {

auto read_text_file(const std::filesystem::path& path) -> std::optional<std::string> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

auto collect_app_ui_sources() -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> files;
    std::array<std::filesystem::path, 2> directories = {
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/app",
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/ui",
    };

    for (const auto& dir : directories) {
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec) || ec) {
            continue;
        }
        for (auto it = std::filesystem::recursive_directory_iterator(dir, ec);
             it != std::filesystem::recursive_directory_iterator() && !ec; it.increment(ec)) {
            if (!it->is_regular_file(ec) || ec) {
                continue;
            }
            auto ext = it->path().extension();
            if (ext == ".cpp" || ext == ".hpp") {
                files.push_back(it->path());
            }
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

auto count_occurrences(std::string_view text, std::string_view needle) -> size_t {
    size_t count = 0;
    size_t pos = text.find(needle);
    while (pos != std::string_view::npos) {
        ++count;
        pos = text.find(needle, pos + needle.size());
    }
    return count;
}

auto surface_token(std::uintptr_t value) -> goggles::input::wlr_surface* {
    return reinterpret_cast<goggles::input::wlr_surface*>(value);
}

} // namespace

TEST_CASE("Filter chain boundary control contract coverage", "[filter_chain][boundary_contract]") {
    using goggles::render::FilterControlDescriptor;
    using goggles::render::FilterControlId;
    using goggles::render::FilterControlStage;
    using goggles::render::VulkanBackend;

    using ListAllSig = std::vector<FilterControlDescriptor> (VulkanBackend::*)() const;
    using ListStageSig =
        std::vector<FilterControlDescriptor> (VulkanBackend::*)(FilterControlStage) const;
    using SetSig = bool (VulkanBackend::*)(FilterControlId, float);
    using ResetSig = bool (VulkanBackend::*)(FilterControlId);

    static_assert(
        std::is_same_v<decltype(static_cast<ListAllSig>(&VulkanBackend::list_filter_controls)),
                       ListAllSig>);
    static_assert(
        std::is_same_v<decltype(static_cast<ListStageSig>(&VulkanBackend::list_filter_controls)),
                       ListStageSig>);
    static_assert(std::is_same_v<decltype(&VulkanBackend::set_filter_control_value), SetSig>);
    static_assert(std::is_same_v<decltype(&VulkanBackend::reset_filter_control_value), ResetSig>);

    static_assert(std::is_same_v<goggles::ui::ParameterChangeCallback,
                                 std::function<void(FilterControlId, float)>>);
    static_assert(std::is_same_v<goggles::ui::PreChainParameterCallback,
                                 std::function<void(FilterControlId, float)>>);

    REQUIRE(std::string_view{goggles::render::to_string(FilterControlStage::prechain)} ==
            "prechain");
    REQUIRE(std::string_view{goggles::render::to_string(FilterControlStage::effect)} == "effect");

    const auto app_path = std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/app/application.cpp";
    auto app_text = read_text_file(app_path);
    REQUIRE(app_text.has_value());
    REQUIRE(app_text->find("list_filter_controls(") != std::string::npos);
    REQUIRE(app_text->find("set_filter_control_value(") != std::string::npos);
    REQUIRE((app_text->find("reset_filter_control_value(") != std::string::npos ||
             app_text->find("reset_filter_controls(") != std::string::npos));
    REQUIRE(app_text->find("filter_chain(") == std::string::npos);

    const auto chain_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/chain/filter_chain.cpp";
    auto chain_text = read_text_file(chain_path);
    REQUIRE(chain_text.has_value());

    const auto imgui_path = std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/ui/imgui_layer.cpp";
    auto imgui_text = read_text_file(imgui_path);
    REQUIRE(imgui_text.has_value());

    const auto backend_context_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/vulkan_context.hpp";
    auto backend_context_text = read_text_file(backend_context_path);
    REQUIRE(backend_context_text.has_value());
    REQUIRE(backend_context_text->find("render/chain/vulkan_context.hpp") != std::string::npos);
    REQUIRE(backend_context_text->find("boundary_context(") != std::string::npos);

    const auto prechain_list_pos =
        chain_text->find("auto prechain_controls = collect_prechain_controls();");
    const auto effect_list_pos =
        chain_text->find("auto effect_controls = collect_effect_controls();", prechain_list_pos);
    const auto merge_pos = chain_text->find("prechain_controls.insert(", effect_list_pos);
    REQUIRE(prechain_list_pos != std::string::npos);
    REQUIRE(effect_list_pos != std::string::npos);
    REQUIRE(merge_pos != std::string::npos);
    REQUIRE(prechain_list_pos < effect_list_pos);
    REQUIRE(effect_list_pos < merge_pos);

    REQUIRE(
        chain_text->find("const float clamped = clamp_filter_control_value(descriptor, value);") !=
        std::string::npos);
    REQUIRE(chain_text->find("std::round(clamped)") != std::string::npos);

    REQUIRE(imgui_text->find("FILTER_TYPE_LABELS") != std::string::npos);
    REQUIRE(imgui_text->find("\"Nearest\"") != std::string::npos);
    REQUIRE(imgui_text->find("param.name == \"filter_type\"") != std::string::npos);

    const auto source_files = collect_app_ui_sources();
    const std::array<std::string_view, 3> forbidden_patterns = {
        "render/chain/filter_chain.hpp",
        "render/shader/",
        "filter_chain(",
    };

    for (const auto& source_path : source_files) {
        auto source_text = read_text_file(source_path);
        REQUIRE(source_text.has_value());
        for (const auto& pattern : forbidden_patterns) {
            INFO("File: " << source_path << ", pattern: " << pattern);
            REQUIRE(source_text->find(pattern) == std::string::npos);
        }
    }
}

TEST_CASE("Async swap and resize safety contract coverage", "[filter_chain][async_contract]") {
    const auto backend_cpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/vulkan_backend.cpp";
    const auto controller_cpp = std::filesystem::path(GOGGLES_SOURCE_DIR) /
                                "src/render/backend/filter_chain_controller.cpp";
    const auto render_output_cpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/render_output.cpp";
    auto backend_text = read_text_file(backend_cpp);
    auto controller_text = read_text_file(controller_cpp);
    auto render_output_text = read_text_file(render_output_cpp);
    REQUIRE(backend_text.has_value());
    REQUIRE(controller_text.has_value());
    REQUIRE(render_output_text.has_value());
    REQUIRE(backend_text->find("m_vulkan_context.boundary_context(m_render_output.command_pool)") !=
            std::string::npos);

    const auto check_swap_pos =
        controller_text->find("void FilterChainController::check_pending_chain_swap(");
    REQUIRE(check_swap_pos != std::string::npos);

    const auto failure_branch_pos = controller_text->find("if (!result) {", check_swap_pos);
    const auto failure_reset_pos =
        controller_text->find("destroy_filter_chain(pending_filter_chain", failure_branch_pos);
    const auto failure_clear_ready_pos =
        controller_text->find("pending_chain_ready.store(false", failure_reset_pos);
    const auto failure_return_pos = controller_text->find("return;", failure_clear_ready_pos);
    const auto success_signal_pos = controller_text->find(
        "chain_swapped.store(true, std::memory_order_release);", failure_return_pos);

    REQUIRE(failure_branch_pos != std::string::npos);
    REQUIRE(failure_reset_pos != std::string::npos);
    REQUIRE(failure_clear_ready_pos != std::string::npos);
    REQUIRE(failure_return_pos != std::string::npos);
    REQUIRE(success_signal_pos != std::string::npos);
    REQUIRE(failure_branch_pos < failure_reset_pos);
    REQUIRE(failure_reset_pos < failure_clear_ready_pos);
    REQUIRE(failure_clear_ready_pos < failure_return_pos);
    REQUIRE(failure_return_pos < success_signal_pos);

    const auto swap_reapply_resolution_pos = controller_text->find(
        "filter_chain.set_prechain_resolution(source_resolution)", check_swap_pos);
    REQUIRE(swap_reapply_resolution_pos != std::string::npos);
    REQUIRE(swap_reapply_resolution_pos < success_signal_pos);

    REQUIRE(controller_text->find("deferred.destroy_after_frame = retire_after_frame") !=
            std::string::npos);
    REQUIRE(backend_text->find("m_filter_chain_controller.handle_resize(") != std::string::npos);
    REQUIRE(backend_text->find("m_external_frame_importer.import_external_image") !=
            std::string::npos);
    REQUIRE(backend_text->find("m_external_frame_importer.prepare_wait_semaphore") !=
            std::string::npos);
    REQUIRE(backend_text->find("m_external_frame_importer.retire_wait_semaphore") !=
            std::string::npos);
    REQUIRE(backend_text->find("m_render_output.is_headless()") != std::string::npos);
    REQUIRE(backend_text->find("m_vulkan_context.headless") == std::string::npos);
    REQUIRE(backend_text->find("m_render_output.target_extent()") != std::string::npos);
    REQUIRE(backend_text->find("m_render_output.clear_resize_request()") != std::string::npos);
    REQUIRE(render_output_text->find("auto RenderOutput::acquire_next_image") != std::string::npos);
    REQUIRE(render_output_text->find("auto RenderOutput::submit_and_present") != std::string::npos);
    REQUIRE(render_output_text->find("auto RenderOutput::submit_headless") != std::string::npos);
    REQUIRE(render_output_text->find("auto RenderOutput::readback_to_png") != std::string::npos);

    const auto shutdown_pos = backend_text->find("void VulkanBackend::shutdown()");
    const auto controller_shutdown_pos =
        backend_text->find("m_filter_chain_controller.shutdown(", shutdown_pos);
    const auto context_destroy_pos =
        backend_text->find("m_vulkan_context.destroy();", shutdown_pos);

    REQUIRE(shutdown_pos != std::string::npos);
    REQUIRE(controller_shutdown_pos != std::string::npos);
    REQUIRE(context_destroy_pos != std::string::npos);
    REQUIRE(controller_shutdown_pos < context_destroy_pos);

    const auto backend_hpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/vulkan_backend.hpp";
    auto header_text = read_text_file(backend_hpp);
    REQUIRE(header_text.has_value());
    REQUIRE(header_text->find("return m_render_output.needs_resize;") != std::string::npos);
    REQUIRE(header_text->find("return m_filter_chain_controller.consume_chain_swapped();") !=
            std::string::npos);
}

TEST_CASE("Filter chain wrapper boundary contract coverage", "[filter_chain][wrapper_contract]") {
    const auto wrapper_hpp = std::filesystem::path(GOGGLES_SOURCE_DIR) /
                             "src/render/chain/api/cpp/goggles_filter_chain.hpp";
    auto header_text = read_text_file(wrapper_hpp);
    REQUIRE(header_text.has_value());

    REQUIRE(header_text->find("class GOGGLES_CHAIN_CPP_API FilterChainRuntime") !=
            std::string::npos);
    REQUIRE(header_text->find("FilterChainRuntime(const FilterChainRuntime&) = delete;") !=
            std::string::npos);
    REQUIRE(header_text->find(
                "operator=(const FilterChainRuntime&) -> FilterChainRuntime& = delete;") !=
            std::string::npos);
    REQUIRE(header_text->find("FilterChainRuntime(FilterChainRuntime&& other) noexcept") !=
            std::string::npos);
    REQUIRE(header_text->find(
                "operator=(FilterChainRuntime&& other) noexcept -> FilterChainRuntime&") !=
            std::string::npos);
    REQUIRE(header_text->find("goggles_chain_t**") == std::string::npos);

    using goggles::render::ChainStageMask;
    using goggles::render::ChainStagePolicy;
    using goggles::render::stage_policy_mask;

    const auto all_enabled =
        stage_policy_mask(ChainStagePolicy{.prechain_enabled = true, .effect_stage_enabled = true});
    const auto pre_only = stage_policy_mask(
        ChainStagePolicy{.prechain_enabled = true, .effect_stage_enabled = false});
    const auto effect_only = stage_policy_mask(
        ChainStagePolicy{.prechain_enabled = false, .effect_stage_enabled = true});
    const auto output_only = stage_policy_mask(
        ChainStagePolicy{.prechain_enabled = false, .effect_stage_enabled = false});

    REQUIRE((all_enabled & ChainStageMask::postchain) == ChainStageMask::postchain);
    REQUIRE((all_enabled & ChainStageMask::prechain) == ChainStageMask::prechain);
    REQUIRE((all_enabled & ChainStageMask::effect) == ChainStageMask::effect);

    REQUIRE((pre_only & ChainStageMask::postchain) == ChainStageMask::postchain);
    REQUIRE((pre_only & ChainStageMask::prechain) == ChainStageMask::prechain);
    REQUIRE((pre_only & ChainStageMask::effect) == ChainStageMask::none);

    REQUIRE((effect_only & ChainStageMask::postchain) == ChainStageMask::postchain);
    REQUIRE((effect_only & ChainStageMask::prechain) == ChainStageMask::none);
    REQUIRE((effect_only & ChainStageMask::effect) == ChainStageMask::effect);

    REQUIRE((output_only & ChainStageMask::postchain) == ChainStageMask::postchain);
    REQUIRE((output_only & ChainStageMask::prechain) == ChainStageMask::none);
    REQUIRE((output_only & ChainStageMask::effect) == ChainStageMask::none);

    const auto wrapper_cpp = std::filesystem::path(GOGGLES_SOURCE_DIR) /
                             "src/render/chain/api/cpp/goggles_filter_chain.cpp";
    auto wrapper_text = read_text_file(wrapper_cpp);
    REQUIRE(wrapper_text.has_value());

    REQUIRE(wrapper_text->find("goggles_chain_create_vk_ex") != std::string::npos);
    REQUIRE(wrapper_text->find("goggles_chain_destroy") != std::string::npos);
    REQUIRE(wrapper_text->find("goggles_chain_record_vk") != std::string::npos);

    REQUIRE(count_occurrences(*wrapper_text, "GOGGLES_LOG_WARN(") == 2);
    REQUIRE(wrapper_text->find("GOGGLES_LOG_ERROR(") == std::string::npos);
    REQUIRE(wrapper_text->find("GOGGLES_LOG_INFO(") == std::string::npos);
}

TEST_CASE("Runtime metrics keep root ownership while tracking the current capture surface",
          "[app_window][runtime_metrics_contract]") {
    using RuntimeMetricsState = goggles::input::RuntimeMetricsState;

    RuntimeMetricsState metrics{};
    auto* game_root = surface_token(0x1000);
    auto* popup_surface = surface_token(0x2000);
    auto* other_root = surface_token(0x3000);
    const RuntimeMetricsState::CaptureTarget game_capture_target{game_root, game_root};
    const RuntimeMetricsState::CaptureTarget popup_capture_target{game_root, popup_surface};
    const RuntimeMetricsState::CaptureTarget other_capture_target{other_root, other_root};
    const RuntimeMetricsState::CaptureTarget other_popup_capture_target{other_root, popup_surface};

    metrics.game_frame_intervals_ms[0] = 16.0F;
    metrics.compositor_latency_samples_ms[0] = 4.0F;
    metrics.game_frame_interval_count = 3;
    metrics.compositor_latency_count = 2;
    metrics.has_last_game_commit_time = true;
    metrics.has_pending_capture_commit_time = true;
    metrics.snapshot.game_fps = 62.5F;
    metrics.snapshot.compositor_latency_ms = 3.5F;
    metrics.snapshot.game_fps_history[0] = 60.0F;
    metrics.snapshot.compositor_latency_history_ms[0] = 4.0F;
    metrics.snapshot.game_fps_history_count = 1;
    metrics.snapshot.compositor_latency_history_count = 1;

    REQUIRE(metrics.should_reset_for_capture_target(game_capture_target));

    metrics.reset_for_capture_target(game_capture_target);

    REQUIRE_FALSE(metrics.should_reset_for_capture_target(game_capture_target));
    REQUIRE(metrics.should_reset_for_capture_target(popup_capture_target));
    REQUIRE(metrics.should_reset_for_capture_target(other_capture_target));
    REQUIRE(metrics.should_track_surface_commit(game_root));
    REQUIRE_FALSE(metrics.should_track_surface_commit(popup_surface));
    REQUIRE_FALSE(metrics.should_track_surface_commit(other_root));
    REQUIRE_FALSE(metrics.should_track_surface_commit(nullptr));
    REQUIRE(metrics.capture_target_root_surface == game_root);
    REQUIRE(metrics.capture_target_surface == game_root);
    REQUIRE(metrics.game_frame_interval_count == 0);
    REQUIRE(metrics.compositor_latency_count == 0);
    REQUIRE_FALSE(metrics.has_last_game_commit_time);
    REQUIRE_FALSE(metrics.has_pending_capture_commit_time);
    REQUIRE(metrics.snapshot.game_fps == 0.0F);
    REQUIRE(metrics.snapshot.compositor_latency_ms == 0.0F);
    REQUIRE(metrics.snapshot.game_fps_history_count == 0);
    REQUIRE(metrics.snapshot.compositor_latency_history_count == 0);

    metrics.reset_for_capture_target(other_popup_capture_target);

    REQUIRE(metrics.capture_target_root_surface == other_root);
    REQUIRE(metrics.capture_target_surface == popup_surface);
    REQUIRE_FALSE(metrics.should_track_surface_commit(other_root));
    REQUIRE(metrics.should_track_surface_commit(popup_surface));
    REQUIRE_FALSE(metrics.should_track_surface_commit(game_root));
}
