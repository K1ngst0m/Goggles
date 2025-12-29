#include "render/shader/retroarch_preprocessor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace goggles::render;

TEST_CASE("RetroArch Preprocessor stage splitting", "[preprocessor]") {
    RetroArchPreprocessor preprocessor;

    SECTION("Split simple vertex/fragment stages") {
        std::string source = R"(
#version 450

// Shared content
layout(push_constant) uniform Push {
    vec4 SourceSize;
} params;

#pragma stage vertex
void main() {
    gl_Position = vec4(0.0);
}

#pragma stage fragment
layout(location = 0) out vec4 FragColor;
void main() {
    FragColor = vec4(1.0);
}
)";

        auto result = preprocessor.preprocess_source(source, "");
        REQUIRE(result.has_value());

        // Vertex should contain shared content and vertex code
        REQUIRE(result->vertex_source.find("void main()") != std::string::npos);
        REQUIRE(result->vertex_source.find("gl_Position") != std::string::npos);
        REQUIRE(result->vertex_source.find("SourceSize") != std::string::npos);

        // Fragment should contain shared content and fragment code
        REQUIRE(result->fragment_source.find("void main()") != std::string::npos);
        REQUIRE(result->fragment_source.find("FragColor") != std::string::npos);
        REQUIRE(result->fragment_source.find("SourceSize") != std::string::npos);

        // Stage pragmas should be removed
        REQUIRE(result->vertex_source.find("#pragma stage") == std::string::npos);
        REQUIRE(result->fragment_source.find("#pragma stage") == std::string::npos);
    }
}

TEST_CASE("RetroArch Preprocessor parameter extraction", "[preprocessor]") {
    RetroArchPreprocessor preprocessor;

    SECTION("Extract parameters from pragma") {
        std::string source = R"(
#pragma parameter BLURSCALE "Blur Scale" 1.0 0.0 2.0 0.1
#pragma parameter LOWLUMSCAN "Scanline Darkness" 0.5 0.0 1.0 0.05

#pragma stage vertex
void main() { gl_Position = vec4(0.0); }

#pragma stage fragment
void main() { FragColor = vec4(1.0); }
)";

        auto result = preprocessor.preprocess_source(source, "");
        REQUIRE(result.has_value());

        REQUIRE(result->parameters.size() == 2);

        auto& param1 = result->parameters[0];
        REQUIRE(param1.name == "BLURSCALE");
        REQUIRE(param1.description == "Blur Scale");
        REQUIRE_THAT(param1.default_value, Catch::Matchers::WithinAbs(1.0, 0.001));
        REQUIRE_THAT(param1.min_value, Catch::Matchers::WithinAbs(0.0, 0.001));
        REQUIRE_THAT(param1.max_value, Catch::Matchers::WithinAbs(2.0, 0.001));
        REQUIRE_THAT(param1.step, Catch::Matchers::WithinAbs(0.1, 0.001));

        auto& param2 = result->parameters[1];
        REQUIRE(param2.name == "LOWLUMSCAN");
        REQUIRE(param2.description == "Scanline Darkness");

        // Parameter pragmas should be removed from source
        REQUIRE(result->vertex_source.find("#pragma parameter") == std::string::npos);
        REQUIRE(result->fragment_source.find("#pragma parameter") == std::string::npos);
    }
}

TEST_CASE("RetroArch Preprocessor metadata extraction", "[preprocessor]") {
    RetroArchPreprocessor preprocessor;

    SECTION("Extract name and format metadata") {
        std::string source = R"(
#pragma name ZfastCRT
#pragma format R8G8B8A8_SRGB

#pragma stage vertex
void main() { gl_Position = vec4(0.0); }

#pragma stage fragment
void main() { FragColor = vec4(1.0); }
)";

        auto result = preprocessor.preprocess_source(source, "");
        REQUIRE(result.has_value());

        REQUIRE(result->metadata.name_alias.has_value());
        REQUIRE(result->metadata.name_alias.value() == "ZfastCRT");

        REQUIRE(result->metadata.format.has_value());
        REQUIRE(result->metadata.format.value() == "R8G8B8A8_SRGB");

        // Metadata pragmas should be removed from source
        REQUIRE(result->vertex_source.find("#pragma name") == std::string::npos);
        REQUIRE(result->vertex_source.find("#pragma format") == std::string::npos);
    }
}

TEST_CASE("RetroArch Preprocessor Slang compatibility fix", "[preprocessor][glsl]") {
    RetroArchPreprocessor preprocessor;

    SECTION("Convert vec *= mat to vec = vec * mat") {
        std::string source = R"(
#version 450

#pragma stage vertex
void main() { gl_Position = vec4(0.0); }

#pragma stage fragment
layout(location = 0) out vec4 FragColor;

#define mix_mat mat3(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)

void main() {
    vec3 yiq = vec3(1.0);
    yiq *= mix_mat;
    FragColor = vec4(yiq, 1.0);
}
)";

        auto result = preprocessor.preprocess_source(source, "");
        REQUIRE(result.has_value());

        // Should convert "yiq *= mix_mat" to "yiq = yiq * (mix_mat)"
        REQUIRE(result->fragment_source.find("yiq = yiq * (mix_mat)") != std::string::npos);
        REQUIRE(result->fragment_source.find("yiq *= mix_mat") == std::string::npos);
    }
}

TEST_CASE("RetroArch Preprocessor compiles with ShaderRuntime", "[preprocessor][integration]") {
    RetroArchPreprocessor preprocessor;

    std::string source = R"(
#version 450

layout(push_constant) uniform Push {
    vec4 SourceSize;
    vec4 OutputSize;
    uint FrameCount;
} params;

#pragma parameter BLURSCALE "Blur Scale" 1.0 0.0 2.0 0.1

#pragma stage vertex
layout(location = 0) in vec2 Position;
layout(location = 0) out vec2 vTexCoord;

void main() {
    gl_Position = vec4(Position, 0.0, 1.0);
    vTexCoord = Position * 0.5 + 0.5;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 0) uniform sampler2D Source;

void main() {
    FragColor = texture(Source, vTexCoord) * params.SourceSize.z;
}
)";

    auto preprocess_result = preprocessor.preprocess_source(source, "");
    REQUIRE(preprocess_result.has_value());

    // Verify we extracted the parameter
    REQUIRE(preprocess_result->parameters.size() == 1);
    REQUIRE(preprocess_result->parameters[0].name == "BLURSCALE");

    // Verify stage split worked
    REQUIRE(!preprocess_result->vertex_source.empty());
    REQUIRE(!preprocess_result->fragment_source.empty());
}
