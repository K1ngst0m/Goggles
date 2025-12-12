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

// Uniform buffer member layout info
struct UniformMember {
    std::string name;
    size_t offset;
    size_t size;
};

// Uniform buffer block layout info
struct UniformBufferLayout {
    uint32_t binding;
    uint32_t set;
    size_t total_size;
    vk::ShaderStageFlags stage_flags;
    std::vector<UniformMember> members;
};

// Push constant layout info
struct PushConstantLayout {
    size_t total_size;
    vk::ShaderStageFlags stage_flags;
    std::vector<UniformMember> members;
};

// Texture binding info
struct TextureBinding {
    std::string name;
    uint32_t binding;
    uint32_t set;
    vk::ShaderStageFlags stage_flags;
};

// Vertex input attribute info
struct VertexInput {
    std::string name;
    uint32_t location;
    vk::Format format;
    uint32_t offset;
};

// Combined reflection data for a shader pass
struct ReflectionData {
    std::optional<UniformBufferLayout> ubo;
    std::optional<PushConstantLayout> push_constants;
    std::vector<TextureBinding> textures;
    std::vector<VertexInput> vertex_inputs;
};

// Reflects a linked Slang program to extract binding information
// linked: The linked IComponentType from ShaderRuntime (after compose + link)
[[nodiscard]] auto reflect_program(slang::IComponentType* linked) -> Result<ReflectionData>;

// Reflects a single shader stage (vertex or fragment)
// stage: VK_SHADER_STAGE_VERTEX_BIT or VK_SHADER_STAGE_FRAGMENT_BIT
[[nodiscard]] auto reflect_stage(slang::IComponentType* linked,
                                  vk::ShaderStageFlags stage) -> Result<ReflectionData>;

// Merge two ReflectionData, combining stage flags for matching bindings
[[nodiscard]] auto merge_reflection(const ReflectionData& vertex,
                                     const ReflectionData& fragment) -> ReflectionData;

} // namespace goggles::render
