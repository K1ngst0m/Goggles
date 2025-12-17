#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace goggles::render {

// RetroArch semantic types for uniform binding
enum class Semantic : std::uint8_t {
    MVP,                 // Model-View-Projection matrix (UBO)
    SOURCE_SIZE,         // vec4: [width, height, 1/width, 1/height] of source texture
    OUTPUT_SIZE,         // vec4: [width, height, 1/width, 1/height] of output
    ORIGINAL_SIZE,       // vec4: size of original captured frame
    FRAME_COUNT,         // uint: frame counter
    FINAL_VIEWPORT_SIZE, // vec4: final output viewport size
};

// Size vec4 format: [width, height, 1/width, 1/height]
struct SizeVec4 {
    float width;
    float height;
    float inv_width;
    float inv_height;

    [[nodiscard]] auto data() const -> const float* { return &width; }
    [[nodiscard]] static constexpr auto size_bytes() -> size_t { return 4 * sizeof(float); }
};

// Computes a size vec4 from dimensions
[[nodiscard]] inline auto make_size_vec4(uint32_t width, uint32_t height) -> SizeVec4 {
    return {
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .inv_width = 1.0F / static_cast<float>(width),
        .inv_height = 1.0F / static_cast<float>(height),
    };
}

// Identity 4x4 matrix for MVP (column-major)
inline constexpr std::array<float, 16> IDENTITY_MVP = {
    1.0F, 0.0F, 0.0F, 0.0F, // column 0
    0.0F, 1.0F, 0.0F, 0.0F, // column 1
    0.0F, 0.0F, 1.0F, 0.0F, // column 2
    0.0F, 0.0F, 0.0F, 1.0F  // column 3
};

// Uniform buffer layout for RetroArch shaders
// Standard layout: MVP at offset 0
struct RetroArchUBO {
    std::array<float, 16> mvp = IDENTITY_MVP;

    [[nodiscard]] static constexpr auto size_bytes() -> size_t {
        return sizeof(std::array<float, 16>);
    }
};

// Push constant layout for RetroArch shaders
// Typical layout: SourceSize, OutputSize, OriginalSize, FrameCount
struct RetroArchPushConstants {
    SizeVec4 source_size;
    SizeVec4 output_size;
    SizeVec4 original_size;
    uint32_t frame_count;
    std::array<uint32_t, 3> padding; // Align to 16 bytes

    [[nodiscard]] static constexpr auto size_bytes() -> size_t {
        return 3 * SizeVec4::size_bytes() + 4 * sizeof(uint32_t);
    }
};

// Binds semantic values to shader uniforms
class SemanticBinder {
public:
    // Set the source texture dimensions
    void set_source_size(uint32_t width, uint32_t height) {
        m_source_size = make_size_vec4(width, height);
    }

    // Set the output/target dimensions
    void set_output_size(uint32_t width, uint32_t height) {
        m_output_size = make_size_vec4(width, height);
    }

    // Set the original captured frame dimensions
    void set_original_size(uint32_t width, uint32_t height) {
        m_original_size = make_size_vec4(width, height);
    }

    // Set the frame counter
    void set_frame_count(uint32_t count) { m_frame_count = count; }

    // Set a custom MVP matrix (column-major)
    void set_mvp(const std::array<float, 16>& mvp) { m_mvp = mvp; }

    // Get populated UBO data
    [[nodiscard]] auto get_ubo() const -> RetroArchUBO { return RetroArchUBO{.mvp = m_mvp}; }

    // Get populated push constants
    [[nodiscard]] auto get_push_constants() const -> RetroArchPushConstants {
        return RetroArchPushConstants{.source_size = m_source_size,
                                      .output_size = m_output_size,
                                      .original_size = m_original_size,
                                      .frame_count = m_frame_count,
                                      .padding = {0, 0, 0}};
    }

    // Convenience: get source size vec4
    [[nodiscard]] auto source_size() const -> const SizeVec4& { return m_source_size; }

    // Convenience: get output size vec4
    [[nodiscard]] auto output_size() const -> const SizeVec4& { return m_output_size; }

    // Convenience: get original size vec4
    [[nodiscard]] auto original_size() const -> const SizeVec4& { return m_original_size; }

    // Convenience: get frame count
    [[nodiscard]] auto frame_count() const -> uint32_t { return m_frame_count; }

private:
    std::array<float, 16> m_mvp = IDENTITY_MVP;
    SizeVec4 m_source_size = {.width = 1.0F, .height = 1.0F, .inv_width = 1.0F, .inv_height = 1.0F};
    SizeVec4 m_output_size = {.width = 1.0F, .height = 1.0F, .inv_width = 1.0F, .inv_height = 1.0F};
    SizeVec4 m_original_size = {
        .width = 1.0F, .height = 1.0F, .inv_width = 1.0F, .inv_height = 1.0F};
    uint32_t m_frame_count = 0;
};

} // namespace goggles::render
