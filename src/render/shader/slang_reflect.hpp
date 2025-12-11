#pragma once

#include <cstdint>
#include <optional>
#include <slang-com-ptr.h>
#include <slang.h>
#include <string>
#include <util/error.hpp>
#include <vector>

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
    std::vector<UniformMember> members;
};

// Push constant layout info
struct PushConstantLayout {
    size_t total_size;
    std::vector<UniformMember> members;
};

// Texture binding info
struct TextureBinding {
    std::string name;
    uint32_t binding;
    uint32_t set;
};

// Combined reflection data for a shader pass
struct ReflectionData {
    std::optional<UniformBufferLayout> ubo;
    std::optional<PushConstantLayout> push_constants;
    std::vector<TextureBinding> textures;
};

// Reflects a linked Slang program to extract binding information
// linked: The linked IComponentType from ShaderRuntime (after compose + link)
[[nodiscard]] auto reflect_program(slang::IComponentType* linked) -> Result<ReflectionData>;

} // namespace goggles::render
