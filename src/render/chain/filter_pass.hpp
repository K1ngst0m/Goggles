#pragma once

#include "pass.hpp"
#include "preset_parser.hpp"
#include "semantic_binder.hpp"

#include <array>
#include <render/shader/retroarch_preprocessor.hpp>
#include <render/shader/slang_reflect.hpp>
#include <unordered_map>
#include <vector>

namespace goggles::render {

/// @brief Configuration for building a `FilterPass` from preprocessed shader sources.
struct FilterPassConfig {
    vk::Format target_format = vk::Format::eUndefined;
    uint32_t num_sync_indices = 2;
    std::string vertex_source;
    std::string fragment_source;
    std::string shader_name;
    FilterMode filter_mode = FilterMode::linear;
    bool mipmap = false;
    WrapMode wrap_mode = WrapMode::clamp_to_edge;
    std::vector<ShaderParameter> parameters;
};

/// @brief Fullscreen vertex format for pass rendering.
struct Vertex {
    std::array<float, 4> position;
    std::array<float, 2> texcoord;
};

struct PassTextureBinding {
    vk::ImageView view;
    vk::Sampler sampler;
};

/// @brief A single shader pass in a filter chain.
class FilterPass : public Pass {
public:
    /// @brief Creates a filter pass from compiled shader sources.
    /// @return A pass or an error.
    [[nodiscard]] static auto create(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                                     const FilterPassConfig& config) -> ResultPtr<FilterPass>;

    ~FilterPass() override;

    FilterPass(const FilterPass&) = delete;
    FilterPass& operator=(const FilterPass&) = delete;
    FilterPass(FilterPass&&) = delete;
    FilterPass& operator=(FilterPass&&) = delete;

    /// @brief Releases GPU resources owned by this pass.
    void shutdown() override;
    /// @brief Records commands to render this pass.
    void record(vk::CommandBuffer cmd, const PassContext& ctx) override;

    [[nodiscard]] auto get_shader_parameters() const -> std::vector<ShaderParameter> override;
    void set_shader_parameter(const std::string& name, float value) override;

    void set_source_size(uint32_t width, uint32_t height) {
        m_binder.set_source_size(width, height);
    }
    void set_output_size(uint32_t width, uint32_t height) {
        m_binder.set_output_size(width, height);
    }
    void set_original_size(uint32_t width, uint32_t height) {
        m_binder.set_original_size(width, height);
    }
    void set_frame_count(uint32_t count, uint32_t mod = 0) {
        uint32_t effective = (mod > 0) ? (count % mod) : count;
        m_binder.set_frame_count(effective);
    }
    void set_rotation(uint32_t rotation) { m_binder.set_rotation(rotation); }
    void set_final_viewport_size(uint32_t width, uint32_t height) {
        m_binder.set_final_viewport_size(width, height);
    }
    void set_alias_size(const std::string& alias, uint32_t width, uint32_t height) {
        m_binder.set_alias_size(alias, width, height);
    }
    void clear_alias_sizes() { m_binder.clear_alias_sizes(); }

    void set_texture_binding(const std::string& name, vk::ImageView view, vk::Sampler sampler) {
        m_texture_bindings[name] = {.view = view, .sampler = sampler};
    }
    void clear_texture_bindings() { m_texture_bindings.clear(); }

    void set_parameter_override(const std::string& name, float value) {
        m_parameter_overrides[name] = value;
    }
    void clear_parameter_overrides() { m_parameter_overrides.clear(); }

    [[nodiscard]] auto parameters() const -> const std::vector<ShaderParameter>& {
        return m_parameters;
    }
    [[nodiscard]] auto get_parameter_value(const std::string& name) const -> float;

    /// @brief Updates the parameter UBO for the current overrides.
    [[nodiscard]] auto update_ubo_parameters() -> Result<void>;
    /// @brief Updates semantic UBO/push constant values.
    void update_ubo_semantics();

    [[nodiscard]] auto texture_bindings() const -> const std::vector<TextureBinding>& {
        return m_merged_reflection.textures;
    }

private:
    FilterPass() = default;
    [[nodiscard]] auto create_descriptor_resources() -> Result<void>;
    [[nodiscard]] auto create_pipeline_layout() -> Result<void>;
    [[nodiscard]] auto create_pipeline(const std::vector<uint32_t>& vertex_spirv,
                                       const std::vector<uint32_t>& fragment_spirv) -> Result<void>;
    [[nodiscard]] auto create_sampler(FilterMode filter_mode, bool mipmap, WrapMode wrap_mode)
        -> Result<void>;
    [[nodiscard]] auto create_vertex_buffer() -> Result<void>;
    [[nodiscard]] auto create_ubo_buffer() -> Result<void>;

    void update_descriptor(uint32_t frame_index, vk::ImageView source_view);
    void build_push_constants();
    [[nodiscard]] auto find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties)
        -> uint32_t;

    vk::Device m_device;
    vk::PhysicalDevice m_physical_device;
    vk::Format m_target_format = vk::Format::eUndefined;
    uint32_t m_num_sync_indices = 0;

    vk::PipelineLayout m_pipeline_layout;
    vk::Pipeline m_pipeline;

    vk::DescriptorSetLayout m_descriptor_layout;
    vk::DescriptorPool m_descriptor_pool;
    std::vector<vk::DescriptorSet> m_descriptor_sets;

    vk::Sampler m_sampler;
    vk::Buffer m_vertex_buffer;
    vk::DeviceMemory m_vertex_buffer_memory;
    vk::Buffer m_ubo_buffer;
    vk::DeviceMemory m_ubo_memory;
    bool m_has_ubo = false;

    SemanticBinder m_binder;

    ReflectionData m_vertex_reflection;
    ReflectionData m_fragment_reflection;
    ReflectionData m_merged_reflection;

    uint32_t m_push_constant_size = 0;
    bool m_has_push_constants = false;
    bool m_has_vertex_inputs = false;

    std::vector<uint8_t> m_push_data;
    std::vector<ShaderParameter> m_parameters;
    std::unordered_map<std::string, PassTextureBinding> m_texture_bindings;
    std::unordered_map<std::string, size_t> m_ubo_member_offsets;
    std::unordered_map<std::string, float> m_parameter_overrides;
    size_t m_ubo_size = 0;
};

} // namespace goggles::render
