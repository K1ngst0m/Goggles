#include "render/shader/shader_runtime.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace goggles::render;

TEST_CASE("ShaderRuntime initialization", "[shader]") {
    ShaderRuntime runtime;

    SECTION("Initialize creates dual sessions") {
        auto result = runtime.init();
        REQUIRE(result.has_value());
        REQUIRE(runtime.is_initialized());
    }

    SECTION("Double init is safe") {
        auto result1 = runtime.init();
        REQUIRE(result1.has_value());
        auto result2 = runtime.init();
        REQUIRE(result2.has_value());
    }

    SECTION("Shutdown and reinitialize") {
        auto result = runtime.init();
        REQUIRE(result.has_value());
        runtime.shutdown();
        REQUIRE(!runtime.is_initialized());
        auto result2 = runtime.init();
        REQUIRE(result2.has_value());
        REQUIRE(runtime.is_initialized());
    }
}

TEST_CASE("ShaderRuntime GLSL compilation", "[shader][glsl]") {
    ShaderRuntime runtime;
    REQUIRE(runtime.init().has_value());

    SECTION("Compile simple GLSL vertex shader") {
        // Minimal GLSL vertex shader
        const std::string vertex_source = R"(
#version 450

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 TexCoord;

layout(location = 0) out vec2 vTexCoord;

void main() {
    gl_Position = vec4(Position, 0.0, 1.0);
    vTexCoord = TexCoord;
}
)";

        const std::string fragment_source = R"(
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D Source;

void main() {
    FragColor = texture(Source, vTexCoord);
}
)";

        auto result = runtime.compile_retroarch_shader(vertex_source, fragment_source, "test_glsl");
        if (!result.has_value()) {
            FAIL("Compile failed: " << result.error().message);
        }
        REQUIRE(!result.value().vertex_spirv.empty());
        REQUIRE(!result.value().fragment_spirv.empty());
    }

    SECTION("GLSL shader with push constants") {
        const std::string vertex_source = R"(
#version 450

layout(push_constant) uniform Push {
    vec4 SourceSize;
    vec4 OutputSize;
    uint FrameCount;
} params;

layout(location = 0) in vec2 Position;
layout(location = 0) out vec2 vTexCoord;

void main() {
    gl_Position = vec4(Position, 0.0, 1.0);
    vTexCoord = Position * 0.5 + 0.5;
}
)";

        const std::string fragment_source = R"(
#version 450

layout(push_constant) uniform Push {
    vec4 SourceSize;
    vec4 OutputSize;
    uint FrameCount;
} params;

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = vec4(vTexCoord, float(params.FrameCount) * 0.001, 1.0);
}
)";

        auto result =
            runtime.compile_retroarch_shader(vertex_source, fragment_source, "test_push_const");
        REQUIRE(result.has_value());
        REQUIRE(!result.value().vertex_spirv.empty());
        REQUIRE(!result.value().fragment_spirv.empty());
    }
}

TEST_CASE("ShaderRuntime error handling", "[shader][error]") {
    ShaderRuntime runtime;
    REQUIRE(runtime.init().has_value());

    SECTION("Invalid GLSL syntax produces error") {
        const std::string bad_vertex = R"(
#version 450
void main() {
    this is not valid glsl
}
)";
        const std::string fragment = R"(
#version 450
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vec4(1.0); }
)";

        auto result = runtime.compile_retroarch_shader(bad_vertex, fragment, "test_error");
        REQUIRE(!result.has_value());
        REQUIRE(result.error().code == goggles::ErrorCode::SHADER_COMPILE_FAILED);
    }

    SECTION("Compile before init fails") {
        ShaderRuntime uninitialized_runtime;

        auto result = uninitialized_runtime.compile_retroarch_shader("", "", "test");
        REQUIRE(!result.has_value());
        REQUIRE(result.error().code == goggles::ErrorCode::SHADER_COMPILE_FAILED);
    }
}
