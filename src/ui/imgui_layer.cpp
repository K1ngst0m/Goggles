#include "imgui_layer.hpp"

#include <SDL3/SDL_video.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <compositor/compositor_server.hpp>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <numeric>
#include <util/logging.hpp>
#include <util/paths.hpp>
#include <utility>

namespace goggles::ui {

namespace {

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

void rebuild_fonts(const std::filesystem::path& font_path, float size_pixels, float display_scale) {
    auto& io = ImGui::GetIO();
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

auto ImGuiLayer::create(SDL_Window* window, const ImGuiConfig& config,
                        const util::AppDirs& app_dirs) -> ResultPtr<ImGuiLayer> {
    auto layer = std::unique_ptr<ImGuiLayer>(new ImGuiLayer());
    layer->m_window = window;
    layer->m_instance = config.instance;
    layer->m_physical_device = config.physical_device;
    layer->m_device = config.device;
    layer->m_queue_family = config.queue_family;
    layer->m_queue = config.queue;
    layer->m_swapchain_format = config.swapchain_format;
    layer->m_image_count = config.image_count;
    layer->m_font_path = util::resource_path(app_dirs, "assets/fonts/RobotoMono-Regular.ttf");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    {
        std::error_code ec;
        std::filesystem::create_directories(app_dirs.config_dir, ec);
        if (!ec) {
            const auto ini_path = app_dirs.config_dir / "imgui.ini";
            layer->m_ini_path = ini_path.string();
            io.IniFilename = layer->m_ini_path.c_str();
        } else {
            // Avoid leaking `imgui.ini` into the working directory if we can't resolve a writable
            // path.
            io.IniFilename = nullptr;
        }
    }

    ImGui::StyleColorsDark();

    float display_scale = get_display_scale(window);
    layer->m_last_display_scale = display_scale;
    rebuild_fonts(layer->m_font_path, layer->m_font_size_pixels, display_scale);

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

    auto now = std::chrono::steady_clock::now();
    if (m_last_frame_time.time_since_epoch().count() > 0) {
        auto delta = std::chrono::duration<float, std::milli>(now - m_last_frame_time).count();
        m_frame_times[m_frame_idx] = delta;
        m_frame_idx = (m_frame_idx + 1) % K_FRAME_HISTORY_SIZE;
    }
    m_last_frame_time = now;

    if (m_window != nullptr) {
        float display_scale = get_display_scale(m_window);
        if (std::fabs(display_scale - m_last_display_scale) > 0.01F) {
            auto wait_result = m_device.waitIdle();
            if (wait_result != vk::Result::eSuccess) {
                GOGGLES_LOG_WARN("waitIdle failed during ImGui DPI font rebuild: {}",
                                 vk::to_string(wait_result));
            }

            rebuild_fonts(m_font_path, m_font_size_pixels, display_scale);

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

    if (!m_global_visible) {
        return;
    }

    draw_shader_controls();
    draw_app_management();
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

void ImGuiLayer::set_prechain_change_callback(PreChainChangeCallback callback) {
    m_on_prechain_change = std::move(callback);
}

void ImGuiLayer::set_prechain_state(vk::Extent2D resolution, ScaleMode scale_mode,
                                    uint32_t integer_scale) {
    m_state.prechain.target_width = resolution.width;
    m_state.prechain.target_height = resolution.height;
    m_state.prechain.scale_mode = scale_mode;
    m_state.prechain.integer_scale = integer_scale;
    m_state.prechain.dirty = false;

    // Determine profile from height (width=0 means aspect-preserve)
    if (resolution.width == 0 && resolution.height == 0) {
        m_state.prechain.profile = ResolutionProfile::disabled;
    } else if (resolution.width == 0 && resolution.height == 240) {
        m_state.prechain.profile = ResolutionProfile::p240;
    } else if (resolution.width == 0 && resolution.height == 288) {
        m_state.prechain.profile = ResolutionProfile::p288;
    } else if (resolution.width == 0 && resolution.height == 480) {
        m_state.prechain.profile = ResolutionProfile::p480;
    } else if (resolution.width == 0 && resolution.height == 720) {
        m_state.prechain.profile = ResolutionProfile::p720;
    } else if (resolution.width == 0 && resolution.height == 1080) {
        m_state.prechain.profile = ResolutionProfile::p1080;
    } else {
        m_state.prechain.profile = ResolutionProfile::custom;
    }
}

void ImGuiLayer::set_prechain_parameters(std::vector<render::ShaderParameter> params) {
    m_state.prechain.pass_parameters = std::move(params);
}

void ImGuiLayer::set_prechain_parameter_callback(PreChainParameterCallback callback) {
    m_on_prechain_parameter = std::move(callback);
}

void ImGuiLayer::set_prechain_scale_mode_callback(PreChainScaleModeCallback callback) {
    m_on_prechain_scale_mode = std::move(callback);
}

void ImGuiLayer::set_surfaces(std::vector<input::SurfaceInfo> surfaces) {
    m_surfaces = std::move(surfaces);
}

void ImGuiLayer::set_manual_override_active(bool active) {
    m_manual_override_active = active;
}

void ImGuiLayer::set_surface_select_callback(SurfaceSelectCallback callback) {
    m_on_surface_select = std::move(callback);
}

void ImGuiLayer::set_surface_reset_callback(SurfaceResetCallback callback) {
    m_on_surface_reset = std::move(callback);
}

void ImGuiLayer::set_pointer_lock_override(bool override_active) {
    m_pointer_lock_override = override_active;
}

void ImGuiLayer::set_pointer_lock_override_callback(PointerLockOverrideCallback callback) {
    m_on_pointer_lock_override = std::move(callback);
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
    ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Shader Controls")) {
        draw_prechain_stage_controls();
        ImGui::Separator();
        draw_effect_stage_controls();
        ImGui::Separator();
        draw_postchain_stage_controls();
    }
    ImGui::End();
}

void ImGuiLayer::draw_prechain_stage_controls() {
    if (ImGui::CollapsingHeader("Pre-Chain Stage", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& prechain = m_state.prechain;

        static constexpr std::array<const char*, 5> SCALE_MODE_LABELS = {
            "Fit", "Fill", "Stretch", "Integer", "Dynamic",
        };
        static constexpr std::array<ScaleMode, 5> SCALE_MODE_VALUES = {
            ScaleMode::fit,     ScaleMode::fill,    ScaleMode::stretch,
            ScaleMode::integer, ScaleMode::dynamic,
        };

        ImGui::Text("Scale Mode:");
        ImGui::SetNextItemWidth(150);
        int mode_index = 0;
        for (size_t i = 0; i < SCALE_MODE_VALUES.size(); ++i) {
            if (SCALE_MODE_VALUES[i] == prechain.scale_mode) {
                mode_index = static_cast<int>(i);
                break;
            }
        }
        if (ImGui::Combo("##scale_mode", &mode_index, SCALE_MODE_LABELS.data(),
                         static_cast<int>(SCALE_MODE_LABELS.size()))) {
            prechain.scale_mode = SCALE_MODE_VALUES[static_cast<size_t>(mode_index)];
            if (m_on_prechain_scale_mode) {
                m_on_prechain_scale_mode(prechain.scale_mode, prechain.integer_scale);
            }
        }

        if (prechain.scale_mode == ScaleMode::integer) {
            ImGui::Text("Integer Scale:");
            ImGui::SetNextItemWidth(120);
            int integer_scale = static_cast<int>(prechain.integer_scale);
            if (ImGui::SliderInt("##integer_scale", &integer_scale, 0, 5)) {
                prechain.integer_scale = static_cast<uint32_t>(std::clamp(integer_scale, 0, 5));
                if (m_on_prechain_scale_mode) {
                    m_on_prechain_scale_mode(prechain.scale_mode, prechain.integer_scale);
                }
            }
        }

        // Resolution profile labels and heights (width=0 preserves aspect ratio)
        static constexpr std::array<const char*, 8> PROFILE_LABELS = {
            "Disabled", // 0: pass-through
            "240p",     // 1: NES, SNES, Genesis, N64, PS1, Saturn
            "288p",     // 2: PS2 240p mode, Wii VC
            "480p",     // 3: Dreamcast, GameCube, PS2, Xbox, Wii
            "480i",     // 4: interlaced variant
            "720p",     // 5: Xbox 360, PS3, Wii U era
            "1080p",    // 6: PS3/360+, modern HD
            "Custom",   // 7: user-defined
        };
        static constexpr std::array<uint32_t, 8> PROFILE_HEIGHTS = {
            0,    // disabled
            240,  // 240p
            288,  // 288p
            480,  // 480p
            480,  // 480i
            720,  // 720p
            1080, // 1080p
            0,    // custom (uses manual input)
        };

        ImGui::Text("Resolution Profile:");
        ImGui::SetNextItemWidth(120);
        int profile_idx = static_cast<int>(prechain.profile);
        if (ImGui::Combo("##profile", &profile_idx, PROFILE_LABELS.data(),
                         static_cast<int>(PROFILE_LABELS.size()))) {
            prechain.profile = static_cast<ResolutionProfile>(profile_idx);

            // Auto-apply for non-custom profiles (width=0 preserves aspect ratio)
            if (prechain.profile != ResolutionProfile::custom) {
                uint32_t h = PROFILE_HEIGHTS[static_cast<size_t>(profile_idx)];
                prechain.target_width = 0;
                prechain.target_height = h;
                prechain.dirty = false;
                if (m_on_prechain_change) {
                    m_on_prechain_change(0, h);
                }
            }
        }

        // Custom resolution input (only shown when Custom is selected)
        if (prechain.profile == ResolutionProfile::custom) {
            ImGui::Text("Target Resolution:");
            ImGui::SetNextItemWidth(100);
            int width = static_cast<int>(prechain.target_width);
            if (ImGui::InputInt("##width", &width, 0, 0)) {
                prechain.target_width = static_cast<uint32_t>(std::max(0, width));
                prechain.dirty = true;
            }
            ImGui::SameLine();
            ImGui::Text("x");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            int height = static_cast<int>(prechain.target_height);
            if (ImGui::InputInt("##height", &height, 0, 0)) {
                prechain.target_height = static_cast<uint32_t>(std::max(0, height));
                prechain.dirty = true;
            }

            if (prechain.dirty) {
                ImGui::SameLine();
                if (ImGui::Button("Apply")) {
                    if (m_on_prechain_change) {
                        m_on_prechain_change(prechain.target_width, prechain.target_height);
                    }
                    prechain.dirty = false;
                }
            }
        }

        // Pass parameters (only shown when pre-chain is active)
        if (prechain.profile != ResolutionProfile::disabled && !prechain.pass_parameters.empty()) {
            ImGui::Separator();
            static constexpr std::array<const char*, 2> FILTER_LABELS = {"Area", "Gaussian"};
            for (auto& param : prechain.pass_parameters) {
                bool is_enum = (param.step >= 1.0F) && (param.max_value - param.min_value) <= 10.0F;
                if (is_enum) {
                    int count = static_cast<int>(param.max_value - param.min_value) + 1;
                    int current = static_cast<int>(param.default_value - param.min_value);

                    ImGui::SetNextItemWidth(150);
                    if (ImGui::Combo(param.description.c_str(), &current, FILTER_LABELS.data(),
                                     std::min(count, static_cast<int>(FILTER_LABELS.size())))) {
                        float new_value = param.min_value + static_cast<float>(current);
                        param.default_value = new_value;
                        if (m_on_prechain_parameter) {
                            m_on_prechain_parameter(param.name, new_value);
                        }
                    }
                } else {
                    float value = param.default_value;
                    ImGui::SetNextItemWidth(150);
                    if (ImGui::SliderFloat(param.description.c_str(), &value, param.min_value,
                                           param.max_value)) {
                        param.default_value = value;
                        if (m_on_prechain_parameter) {
                            m_on_prechain_parameter(param.name, value);
                        }
                    }
                }
            }
        }
    }
}

void ImGuiLayer::draw_effect_stage_controls() {
    if (ImGui::CollapsingHeader("Effect Stage (RetroArch)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Shader", &m_state.shader_enabled);

        if (!m_state.current_preset.empty()) {
            ImGui::Text("Current: %s", m_state.current_preset.filename().string().c_str());
        } else {
            ImGui::TextDisabled("No preset loaded");
        }

        ImGui::Separator();

        if (ImGui::TreeNode("Available Presets")) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##search", "Search...", m_state.search_filter.data(),
                                     m_state.search_filter.size());

            ImGui::BeginChild("##preset_tree", ImVec2(0, 150), ImGuiChildFlags_Borders);
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
            ImGui::TreePop();
        }

        if (!m_state.parameters.empty()) {
            draw_parameter_controls();
        }
    }
}

void ImGuiLayer::draw_postchain_stage_controls() {
    if (ImGui::CollapsingHeader("Post-Chain Stage")) {
        ImGui::TextDisabled("Output Blit");
        ImGui::TextDisabled("(No configurable options)");
    }
}

void ImGuiLayer::draw_parameter_controls() {
    if (ImGui::TreeNode("Shader Parameters")) {
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
        ImGui::TreePop();
    }
}

void ImGuiLayer::draw_app_management() {
    ImGui::SetNextWindowPos(ImVec2(370, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Application")) {
        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            float avg_ms = std::accumulate(m_frame_times.begin(), m_frame_times.end(), 0.F) /
                           static_cast<float>(K_FRAME_HISTORY_SIZE);
            float fps = avg_ms > 0.F ? 1000.F / avg_ms : 0.F;

            float src_avg_ms =
                std::accumulate(m_source_frame_times.begin(), m_source_frame_times.end(), 0.F) /
                static_cast<float>(K_FRAME_HISTORY_SIZE);
            float src_fps = src_avg_ms > 0.F ? 1000.F / src_avg_ms : 0.F;

            ImGui::Text("Render: %.1f FPS (%.2f ms)", fps, avg_ms);
            ImGui::PlotLines("##render_ft", m_frame_times.data(),
                             static_cast<int>(K_FRAME_HISTORY_SIZE), static_cast<int>(m_frame_idx),
                             nullptr, 0.F, 33.F, ImVec2(150, 40));
            ImGui::Text("Source: %.1f FPS (%.2f ms)", src_fps, src_avg_ms);
            ImGui::PlotLines(
                "##source_ft", m_source_frame_times.data(), static_cast<int>(K_FRAME_HISTORY_SIZE),
                static_cast<int>(m_source_frame_idx), nullptr, 0.F, 33.F, ImVec2(150, 40));
        }

        if (ImGui::CollapsingHeader("Input", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Force Enable Pointer Lock", &m_pointer_lock_override)) {
                if (m_on_pointer_lock_override) {
                    m_on_pointer_lock_override(m_pointer_lock_override);
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Force pointer lock even when target app doesn't request it");
            }
            if (m_pointer_lock_override) {
                ImGui::TextColored(ImVec4(1.0F, 0.8F, 0.2F, 1.0F),
                                   "Press Ctrl+Alt+Shift+Q to toggle overlay");
            }

            ImGui::Separator();

            ImGui::Text("Input Target");
            if (m_surfaces.empty()) {
                ImGui::TextDisabled("No surfaces connected");
            } else {
                ImGui::Text("Mode: %s", m_manual_override_active ? "Manual" : "Auto");

                for (const auto& surface : m_surfaces) {
                    ImGui::PushID(static_cast<int>(surface.id));

                    bool is_selected = surface.is_input_target;
                    std::string label;
                    if (!surface.title.empty()) {
                        label = surface.title;
                    } else if (!surface.class_name.empty()) {
                        label = surface.class_name;
                    } else {
                        label = surface.is_xwayland ? "XWayland Surface" : "Wayland Surface";
                    }

                    std::string full_label = std::string(is_selected ? "> " : "  ") + label + " [" +
                                             std::to_string(surface.width) + "x" +
                                             std::to_string(surface.height) + "]";

                    if (ImGui::Selectable(full_label.c_str(), is_selected)) {
                        if (m_on_surface_select) {
                            m_on_surface_select(surface.id);
                        }
                    }

                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("ID: %u", surface.id);
                        ImGui::Text("Title: %s",
                                    surface.title.empty() ? "(none)" : surface.title.c_str());
                        ImGui::Text("Class: %s", surface.class_name.empty()
                                                     ? "(none)"
                                                     : surface.class_name.c_str());
                        ImGui::Text("Size: %dx%d", surface.width, surface.height);
                        ImGui::Text("Type: %s", surface.is_xwayland ? "XWayland" : "Wayland");
                        ImGui::EndTooltip();
                    }

                    ImGui::PopID();
                }

                ImGui::Separator();
                if (ImGui::Button("Reset to Auto")) {
                    if (m_on_surface_reset) {
                        m_on_surface_reset();
                    }
                }
            }
        }
    }
    ImGui::End();
}

void ImGuiLayer::notify_source_frame() {
    auto now = std::chrono::steady_clock::now();
    if (m_last_source_frame_time.time_since_epoch().count() > 0) {
        auto delta =
            std::chrono::duration<float, std::milli>(now - m_last_source_frame_time).count();
        m_source_frame_times[m_source_frame_idx] = delta;
        m_source_frame_idx = (m_source_frame_idx + 1) % K_FRAME_HISTORY_SIZE;
    }
    m_last_source_frame_time = now;
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
