#include "filter_chain.hpp"

#include "filter_chain_core.hpp"

#include <cmath>
#include <render/shader/shader_runtime.hpp>

namespace goggles::render {

namespace {

auto make_optional_description(const std::string& description) -> std::optional<std::string> {
    if (description.empty()) {
        return std::nullopt;
    }
    return description;
}

auto normalize_control_value(const FilterControlDescriptor& descriptor, float value) -> float {
    const float clamped = clamp_filter_control_value(descriptor, value);
    if (descriptor.stage == FilterControlStage::prechain && descriptor.name == "filter_type") {
        return std::round(clamped);
    }
    return clamped;
}

} // namespace

auto FilterChain::create(const VulkanContext& vk_ctx, vk::Format swapchain_format,
                         uint32_t num_sync_indices, const FilterChainPaths& paths,
                         vk::Extent2D source_resolution) -> ResultPtr<FilterChain> {
    auto chain = std::unique_ptr<FilterChain>(new FilterChain());

    chain->m_shader_runtime = GOGGLES_TRY(ShaderRuntime::create(paths.cache_dir));
    chain->m_filter_chain = GOGGLES_TRY(
        FilterChainCore::create(vk_ctx, swapchain_format, num_sync_indices,
                                *chain->m_shader_runtime, paths.shader_dir, source_resolution));
    chain->set_stage_policy(true, true);

    return make_result_ptr(std::move(chain));
}

FilterChain::~FilterChain() {
    shutdown();
}

void FilterChain::shutdown() {
    if (m_filter_chain) {
        m_filter_chain->shutdown();
        m_filter_chain.reset();
    }
    if (m_shader_runtime) {
        m_shader_runtime->shutdown();
        m_shader_runtime.reset();
    }
}

auto FilterChain::load_preset(const std::filesystem::path& preset_path) -> Result<void> {
    if (!m_filter_chain) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }
    if (preset_path.empty()) {
        return {};
    }
    return m_filter_chain->load_preset(preset_path);
}

auto FilterChain::handle_resize(vk::Extent2D new_viewport_extent) -> Result<void> {
    if (!m_filter_chain) {
        return {};
    }
    return m_filter_chain->handle_resize(new_viewport_extent);
}

void FilterChain::record(vk::CommandBuffer cmd, vk::Image original_image,
                         vk::ImageView original_view, vk::Extent2D original_extent,
                         vk::ImageView target_view, vk::Extent2D viewport_extent,
                         uint32_t frame_index, ScaleMode scale_mode, uint32_t integer_scale) {
    if (!m_filter_chain) {
        return;
    }
    m_filter_chain->record(cmd, original_image, original_view, original_extent, target_view,
                           viewport_extent, frame_index, scale_mode, integer_scale);
}

void FilterChain::set_stage_policy(bool prechain_enabled, bool effect_stage_enabled) {
    m_prechain_policy_enabled = prechain_enabled;
    m_effect_stage_policy_enabled = effect_stage_enabled;

    if (!m_filter_chain) {
        return;
    }

    m_filter_chain->set_prechain_enabled(m_prechain_policy_enabled);
    m_filter_chain->set_bypass(!m_effect_stage_policy_enabled);
}

void FilterChain::set_prechain_resolution(vk::Extent2D resolution) {
    if (!m_filter_chain) {
        return;
    }
    m_filter_chain->set_prechain_resolution(resolution.width, resolution.height);
}

auto FilterChain::get_prechain_resolution() const -> vk::Extent2D {
    if (!m_filter_chain) {
        return vk::Extent2D{};
    }
    return m_filter_chain->get_prechain_resolution();
}

auto FilterChain::list_controls() const -> std::vector<FilterControlDescriptor> {
    if (!m_filter_chain) {
        return {};
    }

    auto prechain_controls = collect_prechain_controls();
    auto effect_controls = collect_effect_controls();

    prechain_controls.insert(prechain_controls.end(), effect_controls.begin(),
                             effect_controls.end());
    return prechain_controls;
}

auto FilterChain::list_controls(FilterControlStage stage) const
    -> std::vector<FilterControlDescriptor> {
    if (!m_filter_chain) {
        return {};
    }

    if (stage == FilterControlStage::prechain) {
        return collect_prechain_controls();
    }
    return collect_effect_controls();
}

auto FilterChain::set_control_value(FilterControlId control_id, float value) -> bool {
    if (!m_filter_chain) {
        return false;
    }

    for (const auto& descriptor : list_controls()) {
        if (descriptor.control_id != control_id) {
            continue;
        }
        const float normalized = normalize_control_value(descriptor, value);
        if (descriptor.stage == FilterControlStage::prechain) {
            m_filter_chain->set_prechain_parameter(descriptor.name, normalized);
        } else {
            m_filter_chain->set_parameter(descriptor.name, normalized);
        }
        return true;
    }

    return false;
}

auto FilterChain::reset_control_value(FilterControlId control_id) -> bool {
    if (!m_filter_chain) {
        return false;
    }

    for (const auto& descriptor : list_controls()) {
        if (descriptor.control_id != control_id) {
            continue;
        }
        if (descriptor.stage == FilterControlStage::prechain) {
            m_filter_chain->set_prechain_parameter(descriptor.name, descriptor.default_value);
        } else {
            m_filter_chain->set_parameter(descriptor.name, descriptor.default_value);
        }
        return true;
    }

    return false;
}

void FilterChain::reset_controls() {
    if (!m_filter_chain) {
        return;
    }

    m_filter_chain->clear_parameter_overrides();
    for (const auto& descriptor : collect_prechain_controls()) {
        m_filter_chain->set_prechain_parameter(descriptor.name, descriptor.default_value);
    }
}

auto FilterChain::collect_prechain_controls() const -> std::vector<FilterControlDescriptor> {
    auto prechain_parameters = m_filter_chain->get_prechain_parameters();
    std::vector<FilterControlDescriptor> descriptors;
    descriptors.reserve(prechain_parameters.size());

    for (const auto& parameter : prechain_parameters) {
        descriptors.push_back({
            .control_id = make_filter_control_id(FilterControlStage::prechain, parameter.name),
            .stage = FilterControlStage::prechain,
            .name = parameter.name,
            .description = make_optional_description(parameter.description),
            .current_value = parameter.current_value,
            .default_value = parameter.default_value,
            .min_value = parameter.min_value,
            .max_value = parameter.max_value,
            .step = parameter.step,
        });
    }

    return descriptors;
}

auto FilterChain::collect_effect_controls() const -> std::vector<FilterControlDescriptor> {
    auto parameters = m_filter_chain->get_all_parameters();
    std::vector<FilterControlDescriptor> descriptors;
    descriptors.reserve(parameters.size());

    for (const auto& parameter : parameters) {
        descriptors.push_back({
            .control_id = make_filter_control_id(FilterControlStage::effect, parameter.name),
            .stage = FilterControlStage::effect,
            .name = parameter.name,
            .description = make_optional_description(parameter.description),
            .current_value = parameter.current_value,
            .default_value = parameter.default_value,
            .min_value = parameter.min_value,
            .max_value = parameter.max_value,
            .step = parameter.step,
        });
    }

    return descriptors;
}

} // namespace goggles::render
