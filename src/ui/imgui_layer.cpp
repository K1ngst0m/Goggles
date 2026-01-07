#include "imgui_layer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <util/logging.hpp>
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

} // namespace

auto ImGuiLayer::create(SDL_Window* window, const ImGuiConfig& config) -> ResultPtr<ImGuiLayer> {
    auto layer = std::unique_ptr<ImGuiLayer>(new ImGuiLayer());
    layer->m_device = config.device;
    layer->m_swapchain_format = config.swapchain_format;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

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

    GOGGLES_LOG_INFO("ImGui layer initialized");
    return make_result_ptr(std::move(layer));
}

ImGuiLayer::~ImGuiLayer() {
    shutdown();
}

void ImGuiLayer::shutdown() {
    if (m_device) {
        static_cast<void>(m_device.waitIdle());
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
    ImGui_ImplSDL3_ProcessEvent(&event);
}

void ImGuiLayer::begin_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (m_visible) {
        draw_shader_controls();
    }
}

void ImGuiLayer::end_frame() {
    ImGui::Render();
}

void ImGuiLayer::record(vk::CommandBuffer cmd, vk::ImageView target_view, vk::Extent2D extent) {
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

    for (size_t i = 0; i < m_state.preset_catalog.size(); ++i) {
        const auto& path = m_state.preset_catalog[i];
        PresetTreeNode* current = &m_preset_tree;

        for (auto it = path.begin(); it != path.end(); ++it) {
            std::string part = it->string();
            auto next = std::next(it);
            if (next == path.end()) {
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

} // namespace goggles::ui