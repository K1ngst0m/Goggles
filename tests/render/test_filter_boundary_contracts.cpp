#include "compositor/compositor_runtime_metrics.hpp"
#include "render/backend/filter_chain_controller.hpp"
#include "render/backend/vulkan_backend.hpp"
#include "ui/imgui_layer.hpp"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <goggles/filter_chain.h>
#include <goggles/filter_chain.hpp>
#include <goggles/filter_chain/filter_controls.hpp>
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

auto surface_token(std::uintptr_t value) -> goggles::compositor::wlr_surface* {
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    return reinterpret_cast<goggles::compositor::wlr_surface*>(value);
}

} // namespace

TEST_CASE("Filter chain boundary control contract coverage", "[filter_chain][boundary_contract]") {
    using goggles::fc::FilterControlDescriptor;
    using goggles::fc::FilterControlId;
    using goggles::fc::FilterControlStage;
    using goggles::render::VulkanBackend;

    using FCC = goggles::render::backend_internal::FilterChainController;
    using ListAllSig = std::vector<FilterControlDescriptor> (FCC::*)() const;
    using ListStageSig = std::vector<FilterControlDescriptor> (FCC::*)(FilterControlStage) const;
    using SetSig = bool (FCC::*)(FilterControlId, float);
    using ResetSig = bool (FCC::*)(FilterControlId);

    static_assert(
        std::is_same_v<decltype(static_cast<ListAllSig>(&FCC::list_filter_controls)), ListAllSig>);
    static_assert(std::is_same_v<decltype(static_cast<ListStageSig>(&FCC::list_filter_controls)),
                                 ListStageSig>);
    static_assert(std::is_same_v<decltype(&FCC::set_filter_control_value), SetSig>);
    static_assert(std::is_same_v<decltype(&FCC::reset_filter_control_value), ResetSig>);

    static_assert(std::is_same_v<goggles::ui::ParameterChangeCallback,
                                 std::function<void(FilterControlId, float)>>);
    static_assert(std::is_same_v<goggles::ui::PreChainParameterCallback,
                                 std::function<void(FilterControlId, float)>>);
    static_assert(
        std::is_same_v<goggles::ui::TargetFpsChangeCallback, std::function<void(uint32_t)>>);
    static_assert(std::is_same_v<decltype(goggles::ui::ParameterState{}.descriptor),
                                 FilterControlDescriptor>);
    static_assert(std::is_same_v<decltype(goggles::ui::PreChainState{}.pass_parameters),
                                 std::vector<FilterControlDescriptor>>);

    using SetParametersSig =
        void (goggles::ui::ImGuiLayer::*)(std::vector<goggles::ui::ParameterState>);
    using SetPrechainParametersSig =
        void (goggles::ui::ImGuiLayer::*)(std::vector<FilterControlDescriptor>);
    static_assert(
        std::is_same_v<decltype(&goggles::ui::ImGuiLayer::set_parameters), SetParametersSig>);
    static_assert(std::is_same_v<decltype(&goggles::ui::ImGuiLayer::set_prechain_parameters),
                                 SetPrechainParametersSig>);

    REQUIRE(std::string_view{goggles::fc::to_string(FilterControlStage::prechain)} == "prechain");
    REQUIRE(std::string_view{goggles::fc::to_string(FilterControlStage::effect)} == "effect");

    const auto app_path = std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/app/application.cpp";
    auto app_text = read_text_file(app_path);
    REQUIRE(app_text.has_value());
    REQUIRE(app_text->find("list_filter_controls(") != std::string::npos);
    REQUIRE(app_text->find("set_filter_control_value(") != std::string::npos);
    REQUIRE((app_text->find("reset_filter_control_value(") != std::string::npos ||
             app_text->find("reset_filter_controls(") != std::string::npos));
    REQUIRE(app_text->find("m_filter_chain_controller.") == std::string::npos);

    const auto imgui_path = std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/ui/imgui_layer.cpp";
    auto imgui_text = read_text_file(imgui_path);
    REQUIRE(imgui_text.has_value());

    const auto backend_context_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/vulkan_context.hpp";
    auto backend_context_text = read_text_file(backend_context_path);
    REQUIRE(backend_context_text.has_value());
    REQUIRE(backend_context_text->find("goggles/filter_chain/vulkan_context.hpp") !=
            std::string::npos);
    REQUIRE(backend_context_text->find("boundary_context(") != std::string::npos);

    REQUIRE(imgui_text->find("FILTER_TYPE_LABELS") != std::string::npos);
    REQUIRE(imgui_text->find("\"Nearest\"") != std::string::npos);
    REQUIRE(imgui_text->find("param.name == \"filter_type\"") != std::string::npos);
    REQUIRE(imgui_text->find("set_target_fps_change_callback") != std::string::npos);
    REQUIRE(imgui_text->find("\"Uncapped\"") != std::string::npos);
    REQUIRE(imgui_text->find("\"Effective Pacing Target: Uncapped\"") != std::string::npos);
    REQUIRE(app_text->find("set_target_fps_change_callback") != std::string::npos);
    REQUIRE(app_text->find("m_imgui_layer->set_target_fps(target_fps);") != std::string::npos);

    const auto app_set_target_pos =
        app_text->find("void Application::set_target_fps(uint32_t target_fps)");
    const auto app_assign_target_pos =
        app_text->find("m_target_fps = target_fps;", app_set_target_pos);
    const auto app_imgui_target_pos =
        app_text->find("m_imgui_layer->set_target_fps(target_fps);", app_assign_target_pos);
    const auto app_backend_target_pos =
        app_text->find("m_vulkan_backend->set_target_fps(target_fps);", app_imgui_target_pos);
    const auto app_compositor_target_pos =
        app_text->find("m_compositor_server->set_target_fps(target_fps);", app_backend_target_pos);
    REQUIRE(app_set_target_pos != std::string::npos);
    REQUIRE(app_assign_target_pos != std::string::npos);
    REQUIRE(app_imgui_target_pos != std::string::npos);
    REQUIRE(app_backend_target_pos != std::string::npos);
    REQUIRE(app_compositor_target_pos != std::string::npos);
    REQUIRE(app_assign_target_pos < app_imgui_target_pos);
    REQUIRE(app_imgui_target_pos < app_backend_target_pos);
    REQUIRE(app_backend_target_pos < app_compositor_target_pos);

    const auto uncapped_toggle_pos = imgui_text->find(
        "const uint32_t updated_target_fps = uncapped ? 0u : m_last_capped_target_fps;");
    const auto uncapped_callback_pos =
        imgui_text->find("m_on_target_fps_change(updated_target_fps);", uncapped_toggle_pos);
    const auto capped_input_pos =
        imgui_text->find("ImGui::InputInt(\"Target FPS\"", uncapped_callback_pos);
    const auto capped_clamp_pos = imgui_text->find(
        "static_cast<uint32_t>(std::clamp(capped_target_fps, 1, 1000));", capped_input_pos);
    REQUIRE(uncapped_toggle_pos != std::string::npos);
    REQUIRE(uncapped_callback_pos != std::string::npos);
    REQUIRE(capped_input_pos != std::string::npos);
    REQUIRE(capped_clamp_pos != std::string::npos);
    REQUIRE(uncapped_toggle_pos < capped_input_pos);
    REQUIRE(capped_input_pos < capped_clamp_pos);

    const auto compositor_server_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/compositor/compositor_server.cpp";
    auto compositor_server_text = read_text_file(compositor_server_path);
    REQUIRE(compositor_server_text.has_value());
    const auto compositor_set_target_pos =
        compositor_server_text->find("void CompositorServer::set_target_fps(uint32_t target_fps)");
    const auto compositor_store_pos = compositor_server_text->find(
        "m_state->target_fps.store(target_fps, std::memory_order_release);",
        compositor_set_target_pos);
    const auto compositor_wake_pos =
        compositor_server_text->find("m_state->wake_event_loop();", compositor_store_pos);
    REQUIRE(compositor_set_target_pos != std::string::npos);
    REQUIRE(compositor_store_pos != std::string::npos);
    REQUIRE(compositor_wake_pos != std::string::npos);
    REQUIRE(compositor_store_pos < compositor_wake_pos);

    const auto source_files = collect_app_ui_sources();
    const std::array<std::string_view, 3> forbidden_patterns = {
        "render/chain/chain_runtime.hpp",
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
    REQUIRE(backend_text->find("make_device_info()") != std::string::npos);
    REQUIRE(backend_text->find("make_chain_config()") != std::string::npos);

    const auto check_swap_pos =
        controller_text->find("void FilterChainController::check_pending_chain_swap(");
    REQUIRE(check_swap_pos != std::string::npos);

    const auto failure_branch_pos = controller_text->find("if (!result) {", check_swap_pos);
    const auto failure_reset_pos =
        controller_text->find("shutdown_slot(pending_slot)", failure_branch_pos);
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

    const auto swap_controls_decl_pos =
        controller_text->find("auto controls_result =", check_swap_pos);
    const auto swap_apply_controls_pos =
        controller_text->find("apply_adapter_controls(pending_slot,", swap_controls_decl_pos);
    const auto swap_controls_failure_pos =
        controller_text->find("if (!controls_result) {", swap_apply_controls_pos);
    const auto swap_controls_destroy_pos =
        controller_text->find("shutdown_slot(pending_slot)", swap_controls_failure_pos);
    const auto swap_retire_pos =
        controller_text->find("retire_adapter_with_bounded_fallback(", swap_controls_failure_pos);
    REQUIRE(swap_controls_decl_pos != std::string::npos);
    REQUIRE(swap_apply_controls_pos != std::string::npos);
    REQUIRE(swap_controls_failure_pos != std::string::npos);
    REQUIRE(swap_controls_destroy_pos != std::string::npos);
    REQUIRE(swap_retire_pos != std::string::npos);
    REQUIRE(swap_controls_decl_pos < swap_apply_controls_pos);
    REQUIRE(swap_apply_controls_pos < swap_controls_failure_pos);
    REQUIRE(swap_controls_failure_pos < swap_retire_pos);
    REQUIRE(swap_controls_destroy_pos < swap_retire_pos);
    REQUIRE(swap_retire_pos < success_signal_pos);

    REQUIRE(controller_text->find("requested_controls = authoritative_control_overrides.empty()") !=
            std::string::npos);
    REQUIRE(controller_text->find("apply_adapter_controls(new_adapter, requested_controls,") !=
            std::string::npos);
    REQUIRE(controller_text->find("authoritative_control_overrides = snapshot_adapter_controls(") !=
            std::string::npos);
    REQUIRE(controller_text->find("source_resolution = resolution;") != std::string::npos);
    REQUIRE(controller_text->find("active_slot.chain.set_prechain_resolution(&fc_resolution)") !=
            std::string::npos);
    REQUIRE(controller_text->find("resolve_initial_prechain_resolution") == std::string::npos);

    REQUIRE(controller_text->find("retire_adapter_with_bounded_fallback(") != std::string::npos);
    REQUIRE(
        controller_text->find("cleanup_retired_adapter_tracker(retired_adapters, frame_count);") !=
        std::string::npos);
    REQUIRE(controller_text->find("shutdown_retired_adapter_tracker(retired_adapters);") !=
            std::string::npos);
    REQUIRE(controller_text->find("RetiredAdapterTracker::FALLBACK_RETIRE_DELAY_FRAMES") !=
            std::string::npos);
    REQUIRE(controller_text->find("RetiredAdapterTracker::MAX_RETIRED") != std::string::npos);
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
    REQUIRE(backend_text->find("m_filter_chain_controller.current_prechain_resolution()") !=
            std::string::npos);
    REQUIRE(backend_text->find("m_filter_chain_controller.filter_chain_runtime()") ==
            std::string::npos);
    REQUIRE(backend_text->find("m_filter_chain_controller.record(") != std::string::npos);
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
    REQUIRE(header_text->find("filter_chain_controller()") != std::string::npos);
}

TEST_CASE("Filter chain standalone API boundary contract coverage",
          "[filter_chain][wrapper_contract]") {
    using InstanceCreateSig = goggles::Result<goggles::filter_chain::Instance> (*)(
        const goggles_fc_instance_create_info_t* create_info);
    using DeviceCreateSig = goggles::Result<goggles::filter_chain::Device> (*)(
        goggles::filter_chain::Instance& instance,
        const goggles_fc_vk_device_create_info_t* create_info);
    using ProgramCreateSig = goggles::Result<goggles::filter_chain::Program> (*)(
        goggles::filter_chain::Device& device, const goggles_fc_preset_source_t* source);
    using ChainCreateSig = goggles::Result<goggles::filter_chain::Chain> (*)(
        goggles::filter_chain::Device& device, const goggles::filter_chain::Program& program,
        const goggles_fc_chain_create_info_t* create_info);

    static_assert(
        std::is_same_v<decltype(&goggles::filter_chain::Instance::create), InstanceCreateSig>);
    static_assert(
        std::is_same_v<decltype(&goggles::filter_chain::Device::create), DeviceCreateSig>);
    static_assert(
        std::is_same_v<decltype(&goggles::filter_chain::Program::create), ProgramCreateSig>);
    static_assert(std::is_same_v<decltype(&goggles::filter_chain::Chain::create), ChainCreateSig>);
    static_assert(std::is_move_constructible_v<goggles::filter_chain::Instance>);
    static_assert(std::is_move_constructible_v<goggles::filter_chain::Device>);
    static_assert(std::is_move_constructible_v<goggles::filter_chain::Program>);
    static_assert(std::is_move_constructible_v<goggles::filter_chain::Chain>);
    static_assert(!std::is_copy_constructible_v<goggles::filter_chain::Instance>);
    static_assert(!std::is_copy_constructible_v<goggles::filter_chain::Device>);
    static_assert(!std::is_copy_constructible_v<goggles::filter_chain::Program>);
    static_assert(!std::is_copy_constructible_v<goggles::filter_chain::Chain>);

    const auto c_api_hpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "filter-chain/include/goggles/filter_chain.h";
    const auto canonical_filter_controls_hpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) /
        "filter-chain/include/goggles/filter_chain/filter_controls.hpp";
    const auto canonical_vulkan_context_hpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) /
        "filter-chain/include/goggles/filter_chain/vulkan_context.hpp";
    const auto canonical_error_hpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "filter-chain/common/include/goggles/error.hpp";
    const auto canonical_scale_mode_hpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) /
        "filter-chain/include/goggles/filter_chain/scale_mode.hpp";

    auto c_api_text = read_text_file(c_api_hpp);
    auto canonical_filter_controls_text = read_text_file(canonical_filter_controls_hpp);
    auto canonical_vulkan_context_text = read_text_file(canonical_vulkan_context_hpp);
    auto canonical_error_text = read_text_file(canonical_error_hpp);
    auto canonical_scale_mode_text = read_text_file(canonical_scale_mode_hpp);

    REQUIRE(c_api_text.has_value());
    REQUIRE(canonical_filter_controls_text.has_value());
    REQUIRE(canonical_vulkan_context_text.has_value());
    REQUIRE(canonical_error_text.has_value());
    REQUIRE(canonical_scale_mode_text.has_value());

    REQUIRE(c_api_text->find("util/") == std::string::npos);
    // The goggles_fc_* C header uses the standalone API naming.
    REQUIRE(c_api_text->find("goggles_fc_") != std::string::npos);

    REQUIRE(canonical_filter_controls_text->find("util/") == std::string::npos);
    REQUIRE(canonical_vulkan_context_text->find("util/") == std::string::npos);
    REQUIRE(canonical_error_text->find("util/") == std::string::npos);
    REQUIRE(canonical_scale_mode_text->find("util/") == std::string::npos);

    // Verify the standalone C API stage mask constants cover the expected bit values.
    const uint32_t all_mask = GOGGLES_FC_STAGE_MASK_ALL;
    const uint32_t prechain_mask = GOGGLES_FC_STAGE_MASK_PRECHAIN;
    const uint32_t effect_mask = GOGGLES_FC_STAGE_MASK_EFFECT;
    const uint32_t postchain_mask = GOGGLES_FC_STAGE_MASK_POSTCHAIN;

    REQUIRE((all_mask & postchain_mask) == postchain_mask);
    REQUIRE((all_mask & prechain_mask) == prechain_mask);
    REQUIRE((all_mask & effect_mask) == effect_mask);

    const uint32_t pre_only = prechain_mask | postchain_mask;
    REQUIRE((pre_only & postchain_mask) == postchain_mask);
    REQUIRE((pre_only & prechain_mask) == prechain_mask);
    REQUIRE((pre_only & effect_mask) == 0u);

    const uint32_t effect_only = effect_mask | postchain_mask;
    REQUIRE((effect_only & postchain_mask) == postchain_mask);
    REQUIRE((effect_only & prechain_mask) == 0u);
    REQUIRE((effect_only & effect_mask) == effect_mask);

    const uint32_t output_only_mask = postchain_mask;
    REQUIRE((output_only_mask & postchain_mask) == postchain_mask);
    REQUIRE((output_only_mask & prechain_mask) == 0u);
    REQUIRE((output_only_mask & effect_mask) == 0u);

    REQUIRE(GOGGLES_FC_SCALE_MODE_STRETCH == 0u);
    REQUIRE(GOGGLES_FC_SCALE_MODE_FIT == 1u);
    REQUIRE(GOGGLES_FC_SCALE_MODE_INTEGER == 2u);
    REQUIRE(GOGGLES_FC_SCALE_MODE_FILL == 3u);
    REQUIRE(GOGGLES_FC_SCALE_MODE_DYNAMIC == 4u);

    REQUIRE(static_cast<uint32_t>(goggles::filter_chain::ScaleMode::stretch) ==
            GOGGLES_FC_SCALE_MODE_STRETCH);
    REQUIRE(static_cast<uint32_t>(goggles::filter_chain::ScaleMode::fit) ==
            GOGGLES_FC_SCALE_MODE_FIT);
    REQUIRE(static_cast<uint32_t>(goggles::filter_chain::ScaleMode::integer) ==
            GOGGLES_FC_SCALE_MODE_INTEGER);
    REQUIRE(static_cast<uint32_t>(goggles::filter_chain::ScaleMode::fill) ==
            GOGGLES_FC_SCALE_MODE_FILL);
    REQUIRE(static_cast<uint32_t>(goggles::filter_chain::ScaleMode::dynamic) ==
            GOGGLES_FC_SCALE_MODE_DYNAMIC);
}

TEST_CASE("Runtime metrics keep root ownership while tracking the current capture surface",
          "[app_window][runtime_metrics_contract]") {
    using RuntimeMetricsState = goggles::compositor::RuntimeMetricsState;

    RuntimeMetricsState metrics{};
    auto* game_root = surface_token(0x1000);
    auto* popup_surface = surface_token(0x2000);
    auto* other_root = surface_token(0x3000);
    const RuntimeMetricsState::CaptureTarget game_capture_target{.root_surface = game_root,
                                                                 .surface = game_root};
    const RuntimeMetricsState::CaptureTarget popup_capture_target{.root_surface = game_root,
                                                                  .surface = popup_surface};
    const RuntimeMetricsState::CaptureTarget other_capture_target{.root_surface = other_root,
                                                                  .surface = other_root};
    const RuntimeMetricsState::CaptureTarget other_popup_capture_target{
        .root_surface = other_root,
        .surface = popup_surface,
    };

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

TEST_CASE("Capture pacing only publishes through the paced target callback path",
          "[app_window][capture_pacing_contract]") {
    const auto compositor_present_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/compositor/compositor_present.cpp";
    const auto compositor_state_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/compositor/compositor_state.hpp";

    auto compositor_present_text = read_text_file(compositor_present_path);
    auto compositor_state_text = read_text_file(compositor_state_path);
    REQUIRE(compositor_present_text.has_value());
    REQUIRE(compositor_state_text.has_value());

    const auto non_target_bypass_pos =
        compositor_present_text->find("if (surface != capture_target.surface) {");
    const auto non_target_send_pos =
        compositor_present_text->find("send_frame_done_now(surface);", non_target_bypass_pos);
    const auto non_target_return_pos =
        compositor_present_text->find("return;", non_target_send_pos);
    const auto non_target_update_pos =
        compositor_present_text->find("update_presented_frame(surface);", non_target_bypass_pos);
    REQUIRE(non_target_bypass_pos != std::string::npos);
    REQUIRE(non_target_send_pos != std::string::npos);
    REQUIRE(non_target_return_pos != std::string::npos);
    REQUIRE(non_target_send_pos < non_target_return_pos);
    REQUIRE((non_target_update_pos == std::string::npos ||
             non_target_update_pos > non_target_return_pos));

    REQUIRE(compositor_state_text->find("CompositorState* state = nullptr;") != std::string::npos);
    REQUIRE(compositor_state_text->find("wl_listener callback_surface_destroy{};") !=
            std::string::npos);
    REQUIRE(compositor_present_text->find(
                "track_capture_callback_surface(capture_pacing, surface);") != std::string::npos);
    REQUIRE(compositor_present_text->find("callback_surface_destroy.notify") != std::string::npos);
    REQUIRE(compositor_present_text->find("wl_signal_add(&surface->events.destroy,") !=
            std::string::npos);
    REQUIRE(compositor_present_text->find("capture_pacing->callback_surface = nullptr;") !=
            std::string::npos);
    REQUIRE(compositor_present_text->find("capture_pacing->has_pending_frame = false;") !=
            std::string::npos);
}

TEST_CASE("filter_chain controller uses standalone API boundary",
          "[filter_chain][adapter_boundary]") {
    const auto c_api_hpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "filter-chain/include/goggles/filter_chain.h";
    const auto controller_hpp = std::filesystem::path(GOGGLES_SOURCE_DIR) /
                                "src/render/backend/filter_chain_controller.hpp";
    const auto controller_cpp = std::filesystem::path(GOGGLES_SOURCE_DIR) /
                                "src/render/backend/filter_chain_controller.cpp";
    const auto backend_hpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/vulkan_backend.hpp";
    const auto backend_cpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/vulkan_backend.cpp";

    auto c_api_text = read_text_file(c_api_hpp);
    auto controller_hpp_text = read_text_file(controller_hpp);
    auto controller_cpp_text = read_text_file(controller_cpp);
    auto backend_hpp_text = read_text_file(backend_hpp);
    auto backend_cpp_text = read_text_file(backend_cpp);

    REQUIRE(c_api_text.has_value());
    REQUIRE(controller_hpp_text.has_value());
    REQUIRE(controller_cpp_text.has_value());
    REQUIRE(backend_hpp_text.has_value());
    REQUIRE(backend_cpp_text.has_value());

    SECTION("controller header includes standalone API, not filter-chain internals") {
        // The controller header MUST include only the explicit standalone API headers it uses.
        REQUIRE(controller_hpp_text->find("goggles/filter_chain.hpp") != std::string::npos);
        REQUIRE(controller_hpp_text->find("goggles/filter_chain/filter_controls.hpp") !=
                std::string::npos);
        REQUIRE(controller_hpp_text->find("goggles/filter_chain.h") != std::string::npos);

        // The controller MUST NOT include any filter-chain internal headers.
        REQUIRE(controller_hpp_text->find("filter-chain/src/") == std::string::npos);
        REQUIRE(controller_hpp_text->find("chain_runtime.hpp") == std::string::npos);
        REQUIRE(controller_hpp_text->find("chain_builder.hpp") == std::string::npos);
        REQUIRE(controller_hpp_text->find("chain_resources.hpp") == std::string::npos);
        REQUIRE(controller_hpp_text->find("chain_executor.hpp") == std::string::npos);
        REQUIRE(controller_hpp_text->find("chain_controls.hpp") == std::string::npos);
        REQUIRE(controller_hpp_text->find("shader_runtime.hpp") == std::string::npos);
        REQUIRE(controller_hpp_text->find("texture_loader.hpp") == std::string::npos);
        REQUIRE(controller_hpp_text->find("preset_parser.hpp") == std::string::npos);
        // Check for the internal runtime/chain.hpp using a path-anchored pattern.
        REQUIRE(controller_hpp_text->find("runtime/chain.hpp") == std::string::npos);
        REQUIRE(controller_hpp_text->find("FilterChainRuntime") == std::string::npos);
    }

    SECTION("controller implementation includes standalone API, not internals") {
        // The implementation file must include its own header.
        REQUIRE(controller_cpp_text->find("filter_chain_controller.hpp") != std::string::npos);

        // The implementation must NOT include filter-chain internal headers directly.
        REQUIRE(controller_cpp_text->find("filter-chain/src/") == std::string::npos);
        REQUIRE(controller_cpp_text->find("chain_runtime.hpp") == std::string::npos);
        REQUIRE(controller_cpp_text->find("chain_builder.hpp") == std::string::npos);
        REQUIRE(controller_cpp_text->find("chain_resources.hpp") == std::string::npos);
        REQUIRE(controller_cpp_text->find("shader_runtime.hpp") == std::string::npos);
    }

    SECTION("controller slot owns the goggles_fc_* object graph") {
        // The FilterChainSlot holds the RAII C++ wrappers (Instance, Device, Program, Chain)
        // which in turn call the goggles_fc_* C API.
        REQUIRE(controller_hpp_text->find("goggles::filter_chain::Instance instance") !=
                std::string::npos);
        REQUIRE(controller_hpp_text->find("goggles::filter_chain::Device device") !=
                std::string::npos);
        REQUIRE(controller_hpp_text->find("goggles::filter_chain::Program program") !=
                std::string::npos);
        REQUIRE(controller_hpp_text->find("goggles::filter_chain::Chain chain") !=
                std::string::npos);

        // Verify the slot does NOT own raw goggles_fc_*_t handles — it uses
        // the RAII wrappers which handle destroy automatically.
        REQUIRE(controller_hpp_text->find("goggles_fc_instance_t*") == std::string::npos);
        REQUIRE(controller_hpp_text->find("goggles_fc_device_t*") == std::string::npos);
        REQUIRE(controller_hpp_text->find("goggles_fc_program_t*") == std::string::npos);
        REQUIRE(controller_hpp_text->find("goggles_fc_chain_t*") == std::string::npos);
    }

    SECTION("controller forwards host Vulkan handles without creating its own") {
        // The controller's VulkanDeviceInfo carries host-owned Vulkan handles.
        REQUIRE(controller_hpp_text->find("VkPhysicalDevice physical_device") != std::string::npos);
        REQUIRE(controller_hpp_text->find("VkDevice device") != std::string::npos);
        REQUIRE(controller_hpp_text->find("VkQueue graphics_queue") != std::string::npos);

        // The controller implementation forwards these handles to the standalone API
        // via goggles_fc_vk_device_create_info_init().
        REQUIRE(controller_cpp_text->find("goggles_fc_vk_device_create_info_init()") !=
                std::string::npos);
        REQUIRE(controller_cpp_text->find(
                    "dev_info.physical_device = device_info.physical_device") != std::string::npos);
        REQUIRE(controller_cpp_text->find("dev_info.device = device_info.device") !=
                std::string::npos);
        REQUIRE(controller_cpp_text->find("dev_info.graphics_queue = device_info.graphics_queue") !=
                std::string::npos);

        // The controller must NOT create a VkInstance, VkDevice, or VkQueue internally.
        REQUIRE(controller_cpp_text->find("vkCreateInstance") == std::string::npos);
        REQUIRE(controller_cpp_text->find("vkCreateDevice") == std::string::npos);
    }

    SECTION("controller manages program and chain lifecycle through standalone API") {
        // Program creation goes through the standalone API.
        REQUIRE(controller_cpp_text->find("goggles_fc_preset_source_init()") != std::string::npos);
        REQUIRE(controller_cpp_text->find("goggles::filter_chain::Program::create(") !=
                std::string::npos);
        REQUIRE(controller_cpp_text->find("goggles::filter_chain::Chain::create(") !=
                std::string::npos);

        // Chain operations go through the RAII wrapper methods on the slot.
        REQUIRE(controller_cpp_text->find("slot.chain.record_vk(") != std::string::npos);
        REQUIRE(controller_cpp_text->find("slot.chain.retarget(") != std::string::npos);
        REQUIRE(controller_cpp_text->find("slot.chain.resize(") != std::string::npos);
        REQUIRE(controller_cpp_text->find("slot.chain.get_control_count()") != std::string::npos);
        REQUIRE(controller_cpp_text->find("slot.chain.get_control_info(") != std::string::npos);
        REQUIRE(controller_cpp_text->find("slot.chain.set_control_value_f32(") !=
                std::string::npos);

        // shutdown_slot clears the full object graph in reverse order.
        const auto shutdown_slot_pos = controller_cpp_text->find("void shutdown_slot(");
        REQUIRE(shutdown_slot_pos != std::string::npos);
        const auto chain_clear_pos =
            controller_cpp_text->find("slot.chain = {};", shutdown_slot_pos);
        const auto program_clear_pos =
            controller_cpp_text->find("slot.program = {};", chain_clear_pos);
        const auto device_clear_pos =
            controller_cpp_text->find("slot.device = {};", program_clear_pos);
        const auto instance_clear_pos =
            controller_cpp_text->find("slot.instance = {};", device_clear_pos);
        REQUIRE(chain_clear_pos != std::string::npos);
        REQUIRE(program_clear_pos != std::string::npos);
        REQUIRE(device_clear_pos != std::string::npos);
        REQUIRE(instance_clear_pos != std::string::npos);
        REQUIRE(chain_clear_pos < program_clear_pos);
        REQUIRE(program_clear_pos < device_clear_pos);
        REQUIRE(device_clear_pos < instance_clear_pos);
    }

    SECTION("log forwarding is mediated by controller log_callback") {
        // The controller sets up a static log callback that receives filter-chain log
        // messages and forwards them to the Goggles logging system.
        REQUIRE(controller_cpp_text->find("instance_info.log_callback = &log_callback") !=
                std::string::npos);

        // The callback maps filter-chain log levels to Goggles log macros.
        REQUIRE(controller_cpp_text->find("GOGGLES_FC_LOG_LEVEL_TRACE") != std::string::npos);
        REQUIRE(controller_cpp_text->find("GOGGLES_FC_LOG_LEVEL_DEBUG") != std::string::npos);
        REQUIRE(controller_cpp_text->find("GOGGLES_FC_LOG_LEVEL_INFO") != std::string::npos);
        REQUIRE(controller_cpp_text->find("GOGGLES_FC_LOG_LEVEL_WARN") != std::string::npos);
        REQUIRE(controller_cpp_text->find("GOGGLES_FC_LOG_LEVEL_ERROR") != std::string::npos);
        REQUIRE(controller_cpp_text->find("GOGGLES_FC_LOG_LEVEL_CRITICAL") != std::string::npos);
        REQUIRE(controller_cpp_text->find("GOGGLES_LOG_TRACE(") != std::string::npos);
        REQUIRE(controller_cpp_text->find("GOGGLES_LOG_ERROR(") != std::string::npos);
    }

    SECTION("controller owns slots directly, not filter-chain internals") {
        // The controller owns FilterChainSlot instances (active + pending).
        REQUIRE(controller_hpp_text->find("FilterChainSlot active_slot;") != std::string::npos);
        REQUIRE(controller_hpp_text->find("FilterChainSlot pending_slot;") != std::string::npos);

        // The controller does NOT expose a filter_chain_runtime() accessor.
        REQUIRE(controller_hpp_text->find("filter_chain_runtime()") == std::string::npos);

        // The controller implementation routes all lifecycle through slot helpers.
        REQUIRE(controller_cpp_text->find("initialize_slot(") != std::string::npos);
        REQUIRE(controller_cpp_text->find("shutdown_slot(active_slot)") != std::string::npos);
        REQUIRE(controller_cpp_text->find("shutdown_slot(pending_slot)") != std::string::npos);
    }

    SECTION("Goggles-side code binds only public filter-chain boundary types") {
        using Controller = goggles::render::backend_internal::FilterChainController;
        using ControllerReportSig =
            goggles::Result<goggles_fc_chain_report_t> (Controller::*)() const;
        using ControllerListSig =
            std::vector<goggles::fc::FilterControlDescriptor> (Controller::*)() const;
        using ControllerListStageSig = std::vector<goggles::fc::FilterControlDescriptor> (
            Controller::*)(goggles::fc::FilterControlStage) const;

        static_assert(std::is_same_v<decltype(Controller::FilterChainSlot{}.instance),
                                     goggles::filter_chain::Instance>);
        static_assert(std::is_same_v<decltype(Controller::FilterChainSlot{}.device),
                                     goggles::filter_chain::Device>);
        static_assert(std::is_same_v<decltype(Controller::FilterChainSlot{}.program),
                                     goggles::filter_chain::Program>);
        static_assert(std::is_same_v<decltype(Controller::FilterChainSlot{}.chain),
                                     goggles::filter_chain::Chain>);
        static_assert(std::is_same_v<decltype(&Controller::get_chain_report), ControllerReportSig>);
        static_assert(std::is_same_v<decltype(static_cast<ControllerListSig>(
                                         &Controller::list_filter_controls)),
                                     ControllerListSig>);
        static_assert(std::is_same_v<decltype(static_cast<ControllerListStageSig>(
                                         &Controller::list_filter_controls)),
                                     ControllerListStageSig>);
    }

    SECTION("controller exposes only controller-level types") {
        // The controller class lives in backend_internal namespace.
        REQUIRE(controller_hpp_text->find("namespace goggles::render::backend_internal") !=
                std::string::npos);

        // The controller's public types are VulkanDeviceInfo, ChainConfig, RecordParams —
        // these are controller-level abstractions, not filter-chain internals.
        REQUIRE(controller_hpp_text->find("struct VulkanDeviceInfo") != std::string::npos);
        REQUIRE(controller_hpp_text->find("struct ChainConfig") != std::string::npos);
        REQUIRE(controller_hpp_text->find("struct RecordParams") != std::string::npos);

        // The controller does NOT expose filter-chain internal types in its public interface.
        REQUIRE(controller_hpp_text->find("ChainRuntime") == std::string::npos);
        REQUIRE(controller_hpp_text->find("ChainBuilder") == std::string::npos);
        REQUIRE(controller_hpp_text->find("ChainResources") == std::string::npos);
        REQUIRE(controller_hpp_text->find("ShaderRuntime") == std::string::npos);
        REQUIRE(controller_hpp_text->find("CompiledChain") == std::string::npos);
        REQUIRE(controller_hpp_text->find("PresetParser") == std::string::npos);
    }

    SECTION("retarget goes through controller slot to standalone API") {
        // The controller's align_adapter_output uses goggles_fc_chain_target_info_init.
        REQUIRE(controller_cpp_text->find("goggles_fc_chain_target_info_init()") !=
                std::string::npos);
        REQUIRE(controller_cpp_text->find("target_info.target_format = slot.target_format") !=
                std::string::npos);
        REQUIRE(controller_cpp_text->find("slot.chain.retarget(&target_info)") !=
                std::string::npos);

        // The controller's retarget path uses align_adapter_output.
        REQUIRE(controller_cpp_text->find("align_adapter_output(") != std::string::npos);
    }

    SECTION("prechain resolution update stays distinct from output resize") {
        REQUIRE(c_api_text->find("goggles_fc_chain_set_prechain_resolution(") != std::string::npos);

        const auto controller_prechain_pos =
            controller_cpp_text->find("void FilterChainController::set_prechain_resolution(");
        const auto controller_handle_resize_pos =
            controller_cpp_text->find("auto FilterChainController::handle_resize(");
        REQUIRE(controller_prechain_pos != std::string::npos);
        REQUIRE(controller_handle_resize_pos != std::string::npos);
        // set_prechain_resolution uses set_prechain_resolution API, not resize
        REQUIRE(
            controller_cpp_text->find("active_slot.chain.set_prechain_resolution(&fc_resolution)",
                                      controller_prechain_pos) != std::string::npos);
        // handle_resize uses chain.resize, not set_prechain_resolution
        REQUIRE(controller_cpp_text->find("active_slot.chain.resize(&extent)",
                                          controller_handle_resize_pos) != std::string::npos);
    }

    // Compile-time verification: controller type traits prove Goggles consumes the
    // standalone boundary — the controller is move-only (no accidental copies).
    using Controller = goggles::render::backend_internal::FilterChainController;
    static_assert(!std::is_copy_constructible_v<Controller>, "controller must be move-only");
    static_assert(!std::is_copy_assignable_v<Controller>, "controller must be move-only");
}
