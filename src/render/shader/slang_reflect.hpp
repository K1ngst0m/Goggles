#pragma once

#include <cstdint>
#include <optional>
#include <slang-com-ptr.h>
#include <slang.h>
#include <string>
#include <util/error.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

/// @brief Layout information for a uniform buffer member.
struct UniformMember {
    std::string name;
    size_t offset;
    size_t size;
};

/// @brief Layout information for a uniform buffer block.
struct UniformBufferLayout {
    uint32_t binding;
    uint32_t set;
    size_t total_size;
    vk::ShaderStageFlags stage_flags;
    std::vector<UniformMember> members;
};

/// @brief Layout information for a push constant block.
struct PushConstantLayout {
    size_t total_size;
    vk::ShaderStageFlags stage_flags;
    std::vector<UniformMember> members;
};

/// @brief Descriptor binding information for a sampled texture.
struct TextureBinding {
    std::string name;
    uint32_t binding;
    uint32_t set;
    vk::ShaderStageFlags stage_flags;
};

/// @brief Vertex input attribute metadata.
struct VertexInput {
    std::string name;
    uint32_t location;
    vk::Format format;
    uint32_t offset;
};

/// @brief Combined reflection data for a shader pass.
struct ReflectionData {
    std::optional<UniformBufferLayout> ubo;
    std::optional<PushConstantLayout> push_constants;
    std::vector<TextureBinding> textures;
    std::vector<VertexInput> vertex_inputs;
};

/// @brief Reflects a linked Slang program to extract bindings and layouts.
/// @param linked Linked Slang component (after compose + link).
/// @return Reflection data or an error.
[[nodiscard]] auto reflect_program(slang::IComponentType* linked) -> Result<ReflectionData>;

/// @brief Reflects a single stage from a linked Slang program.
/// @param linked Linked Slang component (after compose + link).
/// @param stage Stage mask (e.g. `vk::ShaderStageFlagBits::eVertex`).
/// @return Reflection data or an error.
[[nodiscard]] auto reflect_stage(slang::IComponentType* linked, vk::ShaderStageFlags stage)
    -> Result<ReflectionData>;

/// @brief Merges two reflection results, combining stage flags for matching bindings.
[[nodiscard]] auto merge_reflection(const ReflectionData& vertex, const ReflectionData& fragment)
    -> ReflectionData;

} // namespace goggles::render
