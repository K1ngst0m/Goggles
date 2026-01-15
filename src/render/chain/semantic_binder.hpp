#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace goggles::render {

/// @brief RetroArch semantic values used for uniform binding.
enum class Semantic : std::uint8_t {
    mvp,
    source_size,
    output_size,
    original_size,
    frame_count,
    final_viewport_size,
};

/// @brief Size vec4 format: `[width, height, 1/width, 1/height]`.
struct SizeVec4 {
    float width;
    float height;
    float inv_width;
    float inv_height;

    [[nodiscard]] auto data() const -> const float* { return &width; }
    [[nodiscard]] static constexpr auto size_bytes() -> size_t { return 4 * sizeof(float); }
};

/// @brief Computes a `SizeVec4` from dimensions.
[[nodiscard]] inline auto make_size_vec4(uint32_t width, uint32_t height) -> SizeVec4 {
    return {
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .inv_width = 1.0F / static_cast<float>(width),
        .inv_height = 1.0F / static_cast<float>(height),
    };
}

/// @brief Identity 4x4 matrix for MVP (column-major).
inline constexpr std::array<float, 16> IDENTITY_MVP = {
    1.0F, 0.0F, 0.0F, 0.0F, // column 0
    0.0F, 1.0F, 0.0F, 0.0F, // column 1
    0.0F, 0.0F, 1.0F, 0.0F, // column 2
    0.0F, 0.0F, 0.0F, 1.0F  // column 3
};

/// @brief Uniform buffer layout for RetroArch shaders (MVP at offset 0).
struct RetroArchUBO {
    std::array<float, 16> mvp = IDENTITY_MVP;

    [[nodiscard]] static constexpr auto size_bytes() -> size_t {
        return sizeof(std::array<float, 16>);
    }
};

/// @brief Push constant layout for RetroArch shaders.
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

/// @brief Binds semantic values to shader uniforms for RetroArch passes.
class SemanticBinder {
public:
    void set_source_size(uint32_t width, uint32_t height) {
        m_source_size = make_size_vec4(width, height);
    }

    void set_output_size(uint32_t width, uint32_t height) {
        m_output_size = make_size_vec4(width, height);
    }

    void set_original_size(uint32_t width, uint32_t height) {
        m_original_size = make_size_vec4(width, height);
    }

    void set_frame_count(uint32_t count) { m_frame_count = count; }

    void set_rotation(uint32_t rotation) { m_rotation = rotation % 4; }

    void set_final_viewport_size(uint32_t width, uint32_t height) {
        m_final_viewport_size = make_size_vec4(width, height);
    }

    // Set a custom MVP matrix (column-major)
    void set_mvp(const std::array<float, 16>& mvp) { m_mvp = mvp; }

    /// @brief Returns the UBO contents for the current semantic values.
    [[nodiscard]] auto get_ubo() const -> RetroArchUBO { return RetroArchUBO{.mvp = m_mvp}; }

    /// @brief Returns push constant contents for the current semantic values.
    [[nodiscard]] auto get_push_constants() const -> RetroArchPushConstants {
        return RetroArchPushConstants{.source_size = m_source_size,
                                      .output_size = m_output_size,
                                      .original_size = m_original_size,
                                      .frame_count = m_frame_count,
                                      .padding = {0, 0, 0}};
    }

    [[nodiscard]] auto source_size() const -> const SizeVec4& { return m_source_size; }

    [[nodiscard]] auto output_size() const -> const SizeVec4& { return m_output_size; }

    [[nodiscard]] auto original_size() const -> const SizeVec4& { return m_original_size; }

    [[nodiscard]] auto frame_count() const -> uint32_t { return m_frame_count; }

    [[nodiscard]] auto rotation() const -> uint32_t { return m_rotation; }

    [[nodiscard]] auto final_viewport_size() const -> const SizeVec4& {
        return m_final_viewport_size;
    }

    void set_alias_size(const std::string& alias, uint32_t width, uint32_t height) {
        m_alias_sizes[alias] = make_size_vec4(width, height);
    }

    /// @brief Returns the `SizeVec4` for an alias, if set.
    [[nodiscard]] auto get_alias_size(const std::string& alias) const -> std::optional<SizeVec4> {
        auto it = m_alias_sizes.find(alias);
        if (it != m_alias_sizes.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void clear_alias_sizes() { m_alias_sizes.clear(); }

private:
    std::array<float, 16> m_mvp = IDENTITY_MVP;
    SizeVec4 m_source_size = {.width = 1.0F, .height = 1.0F, .inv_width = 1.0F, .inv_height = 1.0F};
    SizeVec4 m_output_size = {.width = 1.0F, .height = 1.0F, .inv_width = 1.0F, .inv_height = 1.0F};
    SizeVec4 m_original_size = {
        .width = 1.0F, .height = 1.0F, .inv_width = 1.0F, .inv_height = 1.0F};
    SizeVec4 m_final_viewport_size = {
        .width = 1.0F, .height = 1.0F, .inv_width = 1.0F, .inv_height = 1.0F};
    uint32_t m_frame_count = 0;
    uint32_t m_rotation = 0;
    std::unordered_map<std::string, SizeVec4> m_alias_sizes;
};

} // namespace goggles::render
