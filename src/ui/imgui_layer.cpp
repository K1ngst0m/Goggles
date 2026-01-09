#include "imgui_layer.hpp"

#include <SDL3/SDL_video.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <util/logging.hpp>
#include <utility>

namespace goggles::ui {

namespace {

auto resolve_default_font_path() -> std::filesystem::path {
    if (const char* resource_dir = std::getenv("GOGGLES_RESOURCE_DIR");
        resource_dir && *resource_dir) {
        return std::filesystem::path(resource_dir) / "assets" / "fonts" / "RobotoMono-Regular.ttf";
    }
    return std::filesystem::path("assets") / "fonts" / "RobotoMono-Regular.ttf";
}

auto resolve_imgui_ini_path() -> std::optional<std::filesystem::path> {
    // Allow override for power users / debugging.
    if (const char* ini = std::getenv("GOGGLES_IMGUI_INI"); ini && *ini) {
        return std::filesystem::path(ini);
    }

    std::filesystem::path config_root;
    if (const char* xdg_config = std::getenv("XDG_CONFIG_HOME"); xdg_config && *xdg_config) {
        config_root = xdg_config;
    } else if (const char* home = std::getenv("HOME"); home && *home) {
        config_root = std::filesystem::path(home) / ".config";
    } else {
        return std::nullopt;
    }

    auto dir = config_root / "goggles";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return std::nullopt;
    }

    return dir / "imgui.ini";
}

auto to_lower(std::string_view str) -> std::string {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

auto get_display_scale(SDL_Window* window) -> float {
    if (window == nullptr) {
        return 1.0F;
    }
    float scale = SDL_GetWindowDisplayScale(window);
    if (scale <= 0.0F) {
        return 1.0F;
    }
    return scale;
}

void rebuild_fonts(ImGuiIO& io, const std::filesystem::path& font_path, float size_pixels,
                   float display_scale) {
    io.Fonts->Clear();

    ImFontConfig cfg{};
    cfg.RasterizerDensity = 1.0F;

    ImFont* font = nullptr;
    const float rasterized_size_pixels = size_pixels * display_scale;
    std::error_code ec;
    if (!font_path.empty() && std::filesystem::exists(font_path, ec) && !ec) {
        font =
            io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), rasterized_size_pixels, &cfg);
        if (font == nullptr) {
            GOGGLES_LOG_WARN("Failed to load ImGui font from '{}', falling back to default",
                             font_path.string());
        }
    }

    if (font == nullptr) {
        ImFontConfig default_cfg = cfg;
        default_cfg.SizePixels = rasterized_size_pixels;
        font = io.Fonts->AddFontDefault(&default_cfg);
    }

    io.FontDefault = font;
    io.FontGlobalScale = 1.0F / display_scale;
}

} // namespace

auto ImGuiLayer::create(SDL_Window* window, const ImGuiConfig& config) -> ResultPtr<ImGuiLayer> {
    auto layer = std::unique_ptr<ImGuiLayer>(new ImGuiLayer());
    layer->m_window = window;
    layer->m_instance = config.instance;
    layer->m_physical_device = config.physical_device;
    layer->m_device = config.device;
    layer->m_queue_family = config.queue_family;
    layer->m_queue = config.queue;
    layer->m_swapchain_format = config.swapchain_format;
    layer->m_image_count = config.image_count;
    layer->m_font_path = resolve_default_font_path();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    if (auto ini_path = resolve_imgui_ini_path()) {
        layer->m_ini_path = ini_path->string();
        io.IniFilename = layer->m_ini_path.c_str();
    } else {
        // Avoid leaking `imgui.ini` into the working directory if we can't resolve a writable path.
        io.IniFilename = nullptr;
    }

    ImGui::StyleColorsDark();

    float display_scale = get_display_scale(window);
    layer->m_last_display_scale = display_scale;
    rebuild_fonts(io, layer->m_font_path, layer->m_font_size_pixels, display_scale);

    if (!ImGui_ImplSDL3_InitForVulkan(window)) {
        return make_result_ptr_error<ImGuiLayer>(ErrorCode::vulkan_init_failed,
                                                 "ImGui_ImplSDL3_InitForVulkan failed");
    }

    std::array pool_sizes = {
        vk::DescriptorPoolSize{vk::DescriptorType::eSampler, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformTexelBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageTexelBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBufferDynamic, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eInputAttachment, 1000},
    };

    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = pool_sizes.size();
    pool_info.pPoolSizes = pool_sizes.data();

    auto [pool_result, pool] = config.device.createDescriptorPool(pool_info);
    if (pool_result != vk::Result::eSuccess) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return make_result_ptr_error<ImGuiLayer>(ErrorCode::vulkan_init_failed,
                                                 "Failed to create ImGui descriptor pool");
    }
    layer->m_descriptor_pool = pool;

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = config.instance;
    init_info.PhysicalDevice = config.physical_device;
    init_info.Device = config.device;
    init_info.QueueFamily = config.queue_family;
    init_info.Queue = config.queue;
    init_info.DescriptorPool = layer->m_descriptor_pool;
    init_info.MinImageCount = config.image_count;
    init_info.ImageCount = config.image_count;
    init_info.UseDynamicRendering = true;

    std::array color_formats = {static_cast<VkFormat>(config.swapchain_format)};
    VkPipelineRenderingCreateInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachmentFormats = color_formats.data();
    init_info.PipelineRenderingCreateInfo = rendering_info;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        config.device.destroyDescriptorPool(layer->m_descriptor_pool);
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return make_result_ptr_error<ImGuiLayer>(ErrorCode::vulkan_init_failed,
                                                 "ImGui_ImplVulkan_Init failed");
    }

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        GOGGLES_LOG_WARN("ImGui_ImplVulkan_CreateFontsTexture failed (UI may look wrong on HiDPI)");
    }

    layer->m_initialized = true;
    GOGGLES_LOG_INFO("ImGui layer initialized");
    return make_result_ptr(std::move(layer));
}

ImGuiLayer::~ImGuiLayer() {
    shutdown();
}

void ImGuiLayer::shutdown() {
    if (m_device) {
        auto wait_result = m_device.waitIdle();
        if (wait_result != vk::Result::eSuccess) {
            GOGGLES_LOG_WARN("waitIdle failed in ImGui shutdown: {}", vk::to_string(wait_result));
        }
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        if (m_descriptor_pool) {
            m_device.destroyDescriptorPool(m_descriptor_pool);
            m_descriptor_pool = nullptr;
        }
        m_device = nullptr;
        GOGGLES_LOG_INFO("ImGui layer shutdown");
    }
}

void ImGuiLayer::process_event(const SDL_Event& event) {
    if (!m_initialized) {
        return;
    }
    ImGui_ImplSDL3_ProcessEvent(&event);
}

void ImGuiLayer::begin_frame() {
    if (!m_initialized) {
        return;
    }
    if (m_window != nullptr) {
        float display_scale = get_display_scale(m_window);
        if (std::fabs(display_scale - m_last_display_scale) > 0.01F) {
            auto wait_result = m_device.waitIdle();
            if (wait_result != vk::Result::eSuccess) {
                GOGGLES_LOG_WARN("waitIdle failed during ImGui DPI font rebuild: {}",
                                 vk::to_string(wait_result));
            }

            auto& io = ImGui::GetIO();
            rebuild_fonts(io, m_font_path, m_font_size_pixels, display_scale);

            ImGui_ImplVulkan_DestroyFontsTexture();
            if (!ImGui_ImplVulkan_CreateFontsTexture()) {
                GOGGLES_LOG_WARN(
                    "ImGui_ImplVulkan_CreateFontsTexture failed after DPI change (scale={})",
                    display_scale);
            }

            m_last_display_scale = display_scale;
        }
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (m_visible) {
        draw_shader_controls();
    }
}

void ImGuiLayer::end_frame() {
    if (!m_initialized) {
        return;
    }
    ImGui::Render();
}

void ImGuiLayer::record(vk::CommandBuffer cmd, vk::ImageView target_view, vk::Extent2D extent) {
    if (!m_initialized) {
        return;
    }
    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.imageView = target_view;
    color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_attachment.loadOp = vk::AttachmentLoadOp::eLoad;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;

    vk::RenderingInfo rendering_info{};
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    rendering_info.renderArea.extent = extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    cmd.beginRendering(rendering_info);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    cmd.endRendering();
}

void ImGuiLayer::set_preset_catalog(std::vector<std::filesystem::path> presets) {
    m_state.preset_catalog = std::move(presets);
    rebuild_preset_tree();
}

void ImGuiLayer::rebuild_preset_tree() {
    m_preset_tree = PresetTreeNode{};

    // If presets are absolute (AppImage/XDG), building the UI tree from raw paths
    // produces a confusing root-level hierarchy (/, home, ...). Strip the common
    // parent prefix so the tree starts at the shader-pack root (e.g. crt/...).
    std::filesystem::path common_parent_prefix;
    if (!m_state.preset_catalog.empty()) {
        common_parent_prefix = m_state.preset_catalog[0].parent_path();
        for (size_t i = 1; i < m_state.preset_catalog.size(); ++i) {
            const auto dir = m_state.preset_catalog[i].parent_path();
            std::filesystem::path next_prefix;
            auto it_a = common_parent_prefix.begin();
            auto it_b = dir.begin();
            while (it_a != common_parent_prefix.end() && it_b != dir.end() && *it_a == *it_b) {
                next_prefix /= *it_a;
                ++it_a;
                ++it_b;
            }
            common_parent_prefix = std::move(next_prefix);
            if (common_parent_prefix.empty()) {
                break;
            }
        }
    }

    for (size_t i = 0; i < m_state.preset_catalog.size(); ++i) {
        const auto& path = m_state.preset_catalog[i];

        auto display_path = path;
        if (!common_parent_prefix.empty()) {
            auto rel = path.lexically_relative(common_parent_prefix);
            if (!rel.empty()) {
                display_path = std::move(rel);
            }
        }
        PresetTreeNode* current = &m_preset_tree;

        for (auto it = display_path.begin(); it != display_path.end(); ++it) {
            std::string part = it->string();
            auto next = std::next(it);
            if (next == display_path.end()) {
                current->children[part].preset_index = static_cast<int>(i);
            } else {
                current = &current->children[part];
            }
        }
    }
}

void ImGuiLayer::set_current_preset(const std::filesystem::path& path) {
    m_state.current_preset = path;
    for (size_t i = 0; i < m_state.preset_catalog.size(); ++i) {
        if (m_state.preset_catalog[i] == path) {
            m_state.selected_preset_index = static_cast<int>(i);
            break;
        }
    }
}

void ImGuiLayer::set_parameters(std::vector<ParameterState> params) {
    m_state.parameters = std::move(params);
}

void ImGuiLayer::set_parameter_change_callback(ParameterChangeCallback callback) {
    m_on_parameter_change = std::move(callback);
}

void ImGuiLayer::set_parameter_reset_callback(ParameterResetCallback callback) {
    m_on_parameter_reset = std::move(callback);
}

auto ImGuiLayer::wants_capture_keyboard() const -> bool {
    return ImGui::GetIO().WantCaptureKeyboard;
}

auto ImGuiLayer::wants_capture_mouse() const -> bool {
    return ImGui::GetIO().WantCaptureMouse;
}

auto ImGuiLayer::matches_filter(const std::filesystem::path& path) const -> bool {
    if (m_state.search_filter[0] == '\0') {
        return true;
    }
    auto filename_lower = to_lower(path.filename().string());
    auto filter_lower = to_lower(m_state.search_filter.data());
    return filename_lower.find(filter_lower) != std::string::npos;
}

void ImGuiLayer::draw_filtered_presets() {
    for (size_t i = 0; i < m_state.preset_catalog.size(); ++i) {
        const auto& path = m_state.preset_catalog[i];
        if (!matches_filter(path)) {
            continue;
        }
        ImGui::PushID(static_cast<int>(i));
        bool is_selected = std::cmp_equal(m_state.selected_preset_index, i);
        if (ImGui::Selectable(path.filename().string().c_str(), is_selected)) {
            m_state.selected_preset_index = static_cast<int>(i);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", path.string().c_str());
        }
        ImGui::PopID();
    }
}

void ImGuiLayer::draw_preset_tree(const PresetTreeNode& node) {
    for (const auto& [name, child] : node.children) {
        if (child.preset_index >= 0) {
            bool is_selected = (m_state.selected_preset_index == child.preset_index);
            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (is_selected) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            ImGui::TreeNodeEx(name.c_str(), flags);
            if (ImGui::IsItemClicked()) {
                m_state.selected_preset_index = child.preset_index;
            }
        } else if (ImGui::TreeNode(name.c_str())) {
            draw_preset_tree(child);
            ImGui::TreePop();
        }
    }
}

void ImGuiLayer::draw_shader_controls() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Shader Controls")) {
        ImGui::Checkbox("Enable Shader", &m_state.shader_enabled);

        ImGui::Separator();

        if (!m_state.current_preset.empty()) {
            ImGui::Text("Current: %s", m_state.current_preset.filename().string().c_str());
        } else {
            ImGui::TextDisabled("No preset loaded");
        }

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Available Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##search", "Search...", m_state.search_filter.data(),
                                     m_state.search_filter.size());

            ImGui::BeginChild("##preset_tree", ImVec2(0, 200), ImGuiChildFlags_Borders);
            if (m_state.search_filter[0] == '\0') {
                draw_preset_tree(m_preset_tree);
            } else {
                draw_filtered_presets();
            }
            ImGui::EndChild();

            if (ImGui::Button("Apply Selected")) {
                m_state.shader_enabled = true;
                m_state.reload_requested = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload Current")) {
                m_state.reload_requested = true;
            }
        }

        if (!m_state.parameters.empty()) {
            ImGui::Separator();
            draw_parameter_controls();
        }
    }
    ImGui::End();
}

void ImGuiLayer::draw_parameter_controls() {
    if (ImGui::CollapsingHeader("Shader Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Reset to Defaults")) {
            if (m_on_parameter_reset) {
                m_on_parameter_reset();
            }
        }

        for (auto& param : m_state.parameters) {
            // Skip dummy/separator parameters (min == max)
            if (param.info.min_value >= param.info.max_value) {
                ImGui::TextDisabled("%s", param.info.description.c_str());
                continue;
            }

            float old_value = param.current_value;
            if (ImGui::SliderFloat(param.info.name.c_str(), &param.current_value,
                                   param.info.min_value, param.info.max_value, "%.3f")) {
                if (param.current_value != old_value && m_on_parameter_change) {
                    m_on_parameter_change(param.pass_index, param.info.name, param.current_value);
                }
            }
            if (ImGui::IsItemHovered() && !param.info.description.empty()) {
                ImGui::SetTooltip("%s", param.info.description.c_str());
            }
        }
    }
}

void ImGuiLayer::rebuild_for_format(vk::Format new_format) {
    if (new_format == m_swapchain_format) {
        return;
    }

    GOGGLES_LOG_INFO("rebuild_for_format: {} -> {}", vk::to_string(m_swapchain_format),
                     vk::to_string(new_format));

    auto wait_result = m_device.waitIdle();
    if (wait_result != vk::Result::eSuccess) {
        GOGGLES_LOG_WARN("waitIdle failed during ImGui format rebuild: {}",
                         vk::to_string(wait_result));
    }

    m_initialized = false;
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();

    m_swapchain_format = new_format;

    if (!ImGui_ImplSDL3_InitForVulkan(m_window)) {
        GOGGLES_LOG_ERROR("ImGui_ImplSDL3_InitForVulkan failed during format change, UI disabled");
        return;
    }

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = m_instance;
    init_info.PhysicalDevice = m_physical_device;
    init_info.Device = m_device;
    init_info.QueueFamily = m_queue_family;
    init_info.Queue = m_queue;
    init_info.DescriptorPool = m_descriptor_pool;
    init_info.MinImageCount = m_image_count;
    init_info.ImageCount = m_image_count;
    init_info.UseDynamicRendering = true;

    std::array color_formats = {static_cast<VkFormat>(m_swapchain_format)};
    VkPipelineRenderingCreateInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachmentFormats = color_formats.data();
    init_info.PipelineRenderingCreateInfo = rendering_info;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        ImGui_ImplSDL3_Shutdown();
        GOGGLES_LOG_ERROR("ImGui_ImplVulkan_Init failed during format change, UI disabled");
        return;
    }

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        GOGGLES_LOG_WARN("ImGui_ImplVulkan_CreateFontsTexture failed after format change");
    }

    m_initialized = true;
    GOGGLES_LOG_INFO("ImGui layer rebuilt for format {}", vk::to_string(m_swapchain_format));
}

} // namespace goggles::ui
