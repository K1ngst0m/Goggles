#include "render/shader/shader_runtime.hpp"
#include "render/shader/slang_reflect.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace goggles::render;

TEST_CASE("Slang reflection - texture binding", "[reflection]") {
    ShaderRuntime runtime;
    REQUIRE(runtime.init().has_value());

    // Simple GLSL shader with texture sampler
    std::string vertex_source = R"(#version 450
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main() {
    gl_Position = Position;
    vTexCoord = TexCoord;
}
)";

    std::string fragment_source = R"(#version 450
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(binding = 0, set = 0) uniform sampler2D Source;

void main() {
    FragColor = texture(Source, vTexCoord);
}
)";

    auto result = runtime.compile_retroarch_shader(vertex_source, fragment_source, "test_reflect");
    REQUIRE(result.has_value());

    // Check fragment shader reflection has Source texture
    REQUIRE(result->fragment_reflection.textures.size() == 1);
    REQUIRE(result->fragment_reflection.textures[0].name == "Source");
    REQUIRE(result->fragment_reflection.textures[0].binding == 0);
    REQUIRE(result->fragment_reflection.textures[0].set == 0);
}

TEST_CASE("Slang reflection - push constants", "[reflection]") {
    ShaderRuntime runtime;
    REQUIRE(runtime.init().has_value());

    // GLSL shader with push constants
    std::string vertex_source = R"(#version 450
layout(push_constant) uniform PushConstants {
    vec4 SourceSize;
    vec4 OutputSize;
    uint FrameCount;
} params;

layout(location = 0) in vec4 Position;
layout(location = 0) out vec2 vTexCoord;

void main() {
    gl_Position = Position;
    vTexCoord = Position.xy;
}
)";

    std::string fragment_source = R"(#version 450
layout(push_constant) uniform PushConstants {
    vec4 SourceSize;
    vec4 OutputSize;
    uint FrameCount;
} params;

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = vec4(params.SourceSize.xy / params.OutputSize.xy, 0.0, 1.0);
}
)";

    auto result = runtime.compile_retroarch_shader(vertex_source, fragment_source, "test_push");
    REQUIRE(result.has_value());

    // Both stages should have push constants
    REQUIRE(result->vertex_reflection.push_constants.has_value());
    REQUIRE(result->fragment_reflection.push_constants.has_value());
}

TEST_CASE("Slang reflection - uniform buffer", "[reflection]") {
    ShaderRuntime runtime;
    REQUIRE(runtime.init().has_value());

    // GLSL shader with UBO
    std::string vertex_source = R"(#version 450
layout(binding = 0, set = 0) uniform UBO {
    mat4 MVP;
} ubo;

layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main() {
    gl_Position = ubo.MVP * Position;
    vTexCoord = TexCoord;
}
)";

    std::string fragment_source = R"(#version 450
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = vec4(vTexCoord, 0.0, 1.0);
}
)";

    auto result = runtime.compile_retroarch_shader(vertex_source, fragment_source, "test_ubo");
    REQUIRE(result.has_value());

    // Vertex shader should have UBO
    REQUIRE(result->vertex_reflection.ubo.has_value());
    REQUIRE(result->vertex_reflection.ubo->binding == 0);
    REQUIRE(result->vertex_reflection.ubo->set == 0);
}
