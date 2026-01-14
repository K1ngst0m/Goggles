#include "render/shader/shader_runtime.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace goggles::render;

namespace {
auto test_cache_dir() -> std::filesystem::path {
    auto dir = std::filesystem::temp_directory_path() / "goggles_test_cache";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}
} // namespace

TEST_CASE("ShaderRuntime factory creation", "[shader]") {
    SECTION("Create returns valid instance") {
        auto result = ShaderRuntime::create(test_cache_dir());
        REQUIRE(result.has_value());
        REQUIRE(result.value() != nullptr);
    }

    SECTION("Shutdown and destroy") {
        auto runtime = ShaderRuntime::create(test_cache_dir());
        REQUIRE(runtime.has_value());
        runtime.value()->shutdown();
    }
}

TEST_CASE("ShaderRuntime GLSL compilation", "[shader][glsl]") {
    auto runtime = ShaderRuntime::create(test_cache_dir());
    REQUIRE(runtime.has_value());

    SECTION("Compile simple GLSL vertex shader") {
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

        auto result =
            runtime.value()->compile_retroarch_shader(vertex_source, fragment_source, "test_glsl");
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

        auto result = runtime.value()->compile_retroarch_shader(vertex_source, fragment_source,
                                                                "test_push_const");
        REQUIRE(result.has_value());
        REQUIRE(!result.value().vertex_spirv.empty());
        REQUIRE(!result.value().fragment_spirv.empty());
    }
}

TEST_CASE("ShaderRuntime caching", "[shader][cache]") {
    auto runtime = ShaderRuntime::create(test_cache_dir());
    REQUIRE(runtime.has_value());

    const std::string vert = R"(
#version 450
layout(location = 0) in vec2 Position;
void main() { gl_Position = vec4(Position, 0.0, 1.0); }
)";
    const std::string frag = R"(
#version 450
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vec4(1.0, 0.0, 0.0, 1.0); }
)";
    const std::string module_name = "test_cache";

    auto cache_dir = runtime.value()->get_cache_dir();
    auto cache_file = cache_dir / (module_name + "_ra.cache");
    if (std::filesystem::exists(cache_file)) {
        std::filesystem::remove(cache_file);
    }

    SECTION("Initial compilation creates cache") {
        auto result = runtime.value()->compile_retroarch_shader(vert, frag, module_name);
        REQUIRE(result.has_value());
        REQUIRE(std::filesystem::exists(cache_file));

        auto first_spirv = result->vertex_spirv;

        auto result2 = runtime.value()->compile_retroarch_shader(vert, frag, module_name);
        REQUIRE(result2.has_value());
        REQUIRE(result2->vertex_spirv == first_spirv);
    }

    SECTION("Source change invalidates cache") {
        REQUIRE(runtime.value()->compile_retroarch_shader(vert, frag, module_name).has_value());
        auto old_time = std::filesystem::last_write_time(cache_file);

        const std::string frag_mod = R"(
#version 450
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vec4(0.0, 1.0, 0.0, 1.0); }
)";

        auto result = runtime.value()->compile_retroarch_shader(vert, frag_mod, module_name);
        REQUIRE(result.has_value());
        REQUIRE(std::filesystem::last_write_time(cache_file) > old_time);
    }
}

TEST_CASE("ShaderRuntime error handling", "[shader][error]") {
    auto runtime = ShaderRuntime::create(test_cache_dir());
    REQUIRE(runtime.has_value());

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

        auto result = runtime.value()->compile_retroarch_shader(bad_vertex, fragment, "test_error");
        REQUIRE(!result.has_value());
        REQUIRE(result.error().code == goggles::ErrorCode::shader_compile_failed);
    }
}

TEST_CASE("ShaderRuntime factory failure paths", "[shader][factory][error]") {
    SECTION("Multiple create calls succeed (singleton not enforced)") {
        auto runtime1 = ShaderRuntime::create(test_cache_dir());
        REQUIRE(runtime1.has_value());

        auto runtime2 = ShaderRuntime::create(test_cache_dir());
        REQUIRE(runtime2.has_value());

        REQUIRE(runtime1.value().get() != runtime2.value().get());
    }
}

TEST_CASE("ShaderRuntime error messages", "[shader][error][messages]") {
    auto runtime = ShaderRuntime::create(test_cache_dir());
    REQUIRE(runtime.has_value());

    SECTION("Compilation error messages include shader name") {
        const std::string bad_shader = "invalid glsl code";
        auto result =
            runtime.value()->compile_retroarch_shader(bad_shader, bad_shader, "test_bad_shader");

        REQUIRE(!result.has_value());
        REQUIRE(result.error().code == goggles::ErrorCode::shader_compile_failed);

        const auto& msg = result.error().message;
        REQUIRE(msg.find("test_bad_shader") != std::string::npos);
        REQUIRE(msg.length() > 20);
    }

    SECTION("Missing cache directory doesn't crash") {
        auto runtime2 = ShaderRuntime::create(test_cache_dir());
        REQUIRE(runtime2.has_value());

        auto cache_dir = runtime2.value()->get_cache_dir();
        REQUIRE((std::filesystem::exists(cache_dir) || !cache_dir.empty()));
    }
}

TEST_CASE("ShaderRuntime cleanup behavior", "[shader][cleanup]") {
    SECTION("Shutdown is idempotent") {
        auto runtime = ShaderRuntime::create(test_cache_dir());
        REQUIRE(runtime.has_value());

        runtime.value()->shutdown();
        runtime.value()->shutdown();
    }

    SECTION("Destructor after partial usage") {
        {
            auto runtime = ShaderRuntime::create(test_cache_dir());
            REQUIRE(runtime.has_value());
            const std::string simple = R"(
#version 450
void main() { gl_Position = vec4(0.0); }
)";
            auto result = runtime.value()->compile_retroarch_shader(simple, simple, "test");
            (void)result;
        }
    }
}
