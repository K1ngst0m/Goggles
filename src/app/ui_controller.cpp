#include "ui_controller.hpp"
#include "sdl_platform.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <ranges>
#include <render/backend/vulkan_backend.hpp>
#include <render/chain/filter_chain.hpp>
#include <string>
#include <ui/imgui_layer.hpp>
#include <util/config.hpp>
#include <util/logging.hpp>
#include <utility>
#include <vector>

namespace goggles::app {

static auto to_sdl_window(app::WindowHandle window) -> SDL_Window* {
    return static_cast<SDL_Window*>(window.ptr);
}

static auto to_sdl_event(app::EventRef event) -> const SDL_Event& {
    return *static_cast<const SDL_Event*>(event.ptr);
}

static auto scan_presets(const std::filesystem::path& dir) -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> presets;
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || ec) {
        return presets;
    }
    for (auto it = std::filesystem::recursive_directory_iterator(dir, ec);
         it != std::filesystem::recursive_directory_iterator() && !ec; it.increment(ec)) {
        if (it->is_regular_file(ec) && !ec && it->path().extension() == ".slangp") {
            presets.push_back(it->path());
        }
    }
    std::ranges::sort(presets);
    return presets;
}

static void update_ui_parameters(render::VulkanBackend& vulkan_backend,
                                 ui::ImGuiLayer& imgui_layer) {
    auto* chain = vulkan_backend.filter_chain();
    if (chain == nullptr) {
        return;
    }

    auto params = chain->get_all_parameters();
    std::vector<ui::ParameterState> ui_params;
    ui_params.reserve(params.size());

    for (const auto& p : params) {
        ui_params.push_back({
            .pass_index = 0,
            .info =
                {
                    .name = p.name,
                    .description = p.description,
                    .default_value = p.default_value,
                    .min_value = p.min_value,
                    .max_value = p.max_value,
                    .step = p.step,
                },
            .current_value = p.current_value,
        });
    }
    imgui_layer.set_parameters(std::move(ui_params));
}

auto UiController::create(app::WindowHandle window, render::VulkanBackend& vulkan_backend,
                          const Config& config) -> ResultPtr<UiController> {
    auto controller = std::unique_ptr<UiController>(new UiController());

    auto* sdl_window = to_sdl_window(window);

    ui::ImGuiConfig imgui_config{
        .instance = vulkan_backend.instance(),
        .physical_device = vulkan_backend.physical_device(),
        .device = vulkan_backend.device(),
        .queue_family = vulkan_backend.graphics_queue_family(),
        .queue = vulkan_backend.graphics_queue(),
        .swapchain_format = vulkan_backend.swapchain_format(),
        .image_count = vulkan_backend.swapchain_image_count(),
    };

    auto imgui_result = ui::ImGuiLayer::create(sdl_window, imgui_config);
    if (!imgui_result) {
        GOGGLES_LOG_WARN("ImGui disabled: {}", imgui_result.error().message);
        return make_result_ptr(std::move(controller));
    }

    controller->m_imgui_layer = std::move(imgui_result.value());

    auto presets = scan_presets("shaders/retroarch");
    controller->m_imgui_layer->set_preset_catalog(std::move(presets));
    controller->m_imgui_layer->set_current_preset(vulkan_backend.current_preset_path());
    controller->m_imgui_layer->state().shader_enabled = !config.shader.preset.empty();

    controller->m_imgui_layer->set_parameter_change_callback(
        [&vulkan_backend](size_t /*pass_index*/, const std::string& name, float value) {
            if (auto* chain = vulkan_backend.filter_chain()) {
                chain->set_parameter(name, value);
            }
        });
    controller->m_imgui_layer->set_parameter_reset_callback(
        [&vulkan_backend, ctrl = controller.get()]() {
            if (auto* chain = vulkan_backend.filter_chain()) {
                chain->clear_parameter_overrides();
                if (ctrl->m_imgui_layer) {
                    update_ui_parameters(vulkan_backend, *ctrl->m_imgui_layer);
                }
            }
        });

    update_ui_parameters(vulkan_backend, *controller->m_imgui_layer);
    GOGGLES_LOG_INFO("ImGui layer initialized (F1 to toggle)");

    return make_result_ptr(std::move(controller));
}

UiController::~UiController() {
    shutdown();
}

void UiController::shutdown() {
    m_imgui_layer.reset();
}

auto UiController::enabled() const -> bool {
    return static_cast<bool>(m_imgui_layer);
}

void UiController::process_event(app::EventRef event) {
    if (m_imgui_layer) {
        m_imgui_layer->process_event(to_sdl_event(event));
    }
}

auto UiController::wants_capture_keyboard() const -> bool {
    return m_imgui_layer ? m_imgui_layer->wants_capture_keyboard() : false;
}

auto UiController::wants_capture_mouse() const -> bool {
    return m_imgui_layer ? m_imgui_layer->wants_capture_mouse() : false;
}

void UiController::toggle_visibility() {
    if (m_imgui_layer) {
        m_imgui_layer->toggle_visibility();
    }
}

void UiController::apply_state(render::VulkanBackend& vulkan_backend) {
    if (!m_imgui_layer) {
        return;
    }

    auto& state = m_imgui_layer->state();

    if (state.shader_enabled != m_last_shader_enabled) {
        vulkan_backend.set_shader_enabled(state.shader_enabled);
        m_last_shader_enabled = state.shader_enabled;
    }

    if (!state.reload_requested) {
        return;
    }
    state.reload_requested = false;

    if (state.selected_preset_index < 0 ||
        std::cmp_greater_equal(state.selected_preset_index, state.preset_catalog.size())) {
        return;
    }

    const auto& preset = state.preset_catalog[static_cast<size_t>(state.selected_preset_index)];
    auto result = vulkan_backend.reload_shader_preset(preset);
    if (!result) {
        GOGGLES_LOG_ERROR("Failed to load preset '{}': {}", preset.string(),
                          result.error().message);
    }
}

void UiController::begin_frame() {
    if (m_imgui_layer) {
        m_imgui_layer->begin_frame();
    }
}

void UiController::end_frame() {
    if (m_imgui_layer) {
        m_imgui_layer->end_frame();
    }
}

void UiController::record(vk::CommandBuffer cmd, vk::ImageView target_view, vk::Extent2D extent) {
    if (m_imgui_layer) {
        m_imgui_layer->record(cmd, target_view, extent);
    }
}

void UiController::sync_from_backend(render::VulkanBackend& vulkan_backend) {
    if (!m_imgui_layer) {
        return;
    }
    m_imgui_layer->state().current_preset = vulkan_backend.current_preset_path();
    update_ui_parameters(vulkan_backend, *m_imgui_layer);
}

} // namespace goggles::app
