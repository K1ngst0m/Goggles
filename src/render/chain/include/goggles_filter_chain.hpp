#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <util/error.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

#if defined(_WIN32)
#if defined(GOGGLES_CHAIN_BUILD_SHARED)
#define GOGGLES_CHAIN_CPP_API __declspec(dllexport)
#elif defined(GOGGLES_CHAIN_USE_SHARED)
#define GOGGLES_CHAIN_CPP_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && defined(GOGGLES_CHAIN_BUILD_SHARED)
#define GOGGLES_CHAIN_CPP_API __attribute__((visibility("default")))
#endif

#ifndef GOGGLES_CHAIN_CPP_API
#define GOGGLES_CHAIN_CPP_API
#endif

struct goggles_chain;

namespace goggles::render {

enum class ChainScaleMode : std::uint8_t { stretch, fit, integer };

enum class ChainControlStage : std::uint8_t { prechain, effect, postchain };

enum class ChainStageMask : std::uint8_t {
    none = 0u,
    prechain = 1u << 0,
    effect = 1u << 1,
    postchain = 1u << 2,
    prechain_effect = static_cast<std::uint8_t>(prechain) | static_cast<std::uint8_t>(effect),
    prechain_postchain = static_cast<std::uint8_t>(prechain) | static_cast<std::uint8_t>(postchain),
    effect_postchain = static_cast<std::uint8_t>(effect) | static_cast<std::uint8_t>(postchain),
    all = static_cast<std::uint8_t>(prechain) | static_cast<std::uint8_t>(effect) |
          static_cast<std::uint8_t>(postchain),
};

[[nodiscard]] constexpr auto operator|(ChainStageMask lhs, ChainStageMask rhs) -> ChainStageMask {
    return static_cast<ChainStageMask>(static_cast<std::uint8_t>(lhs) |
                                       static_cast<std::uint8_t>(rhs));
}

[[nodiscard]] constexpr auto operator&(ChainStageMask lhs, ChainStageMask rhs) -> ChainStageMask {
    return static_cast<ChainStageMask>(static_cast<std::uint8_t>(lhs) &
                                       static_cast<std::uint8_t>(rhs));
}

[[nodiscard]] constexpr auto to_underlying(ChainStageMask value) -> std::uint32_t {
    return static_cast<std::uint32_t>(value);
}

struct ChainStagePolicy {
    /// @brief Enable or disable pre-chain stage execution.
    bool prechain_enabled = true;
    /// @brief Enable or disable effect-chain stage execution.
    bool effect_stage_enabled = true;
};

/// @brief Compute stage-enable mask for runtime policy.
/// @note Post-chain/output stage remains always enabled.
[[nodiscard]] constexpr auto stage_policy_mask(const ChainStagePolicy& policy) -> ChainStageMask {
    ChainStageMask stage_mask = ChainStageMask::postchain;
    if (policy.prechain_enabled) {
        stage_mask = stage_mask | ChainStageMask::prechain;
    }
    if (policy.effect_stage_enabled) {
        stage_mask = stage_mask | ChainStageMask::effect;
    }
    return stage_mask;
}

struct ChainCreateInfo {
    vk::Device device;
    vk::PhysicalDevice physical_device;
    vk::Queue graphics_queue;
    std::uint32_t graphics_queue_family_index = 0;
    vk::Format target_format = vk::Format::eUndefined;
    std::uint32_t num_sync_indices = 1;
    std::filesystem::path shader_dir;
    std::filesystem::path cache_dir;
    vk::Extent2D initial_prechain_resolution;
};

struct ChainRecordInfo {
    vk::CommandBuffer command_buffer;
    vk::Image source_image;
    vk::ImageView source_view;
    vk::Extent2D source_extent;
    vk::ImageView target_view;
    vk::Extent2D target_extent;
    std::uint32_t frame_index = 0;
    ChainScaleMode scale_mode = ChainScaleMode::stretch;
    std::uint32_t integer_scale = 1;
};

struct ChainControlDescriptor {
    std::uint64_t control_id = 0;
    ChainControlStage stage = ChainControlStage::effect;
    std::string name;
    std::optional<std::string> description;
    float current_value = 0.0F;
    float default_value = 0.0F;
    float min_value = 0.0F;
    float max_value = 0.0F;
    float step = 0.0F;
};

class GOGGLES_CHAIN_CPP_API FilterChainRuntime {
public:
    FilterChainRuntime() = default;
    ~FilterChainRuntime();

    FilterChainRuntime(const FilterChainRuntime&) = delete;
    auto operator=(const FilterChainRuntime&) -> FilterChainRuntime& = delete;

    FilterChainRuntime(FilterChainRuntime&& other) noexcept;
    auto operator=(FilterChainRuntime&& other) noexcept -> FilterChainRuntime&;

    [[nodiscard]] static auto create(const ChainCreateInfo& create_info)
        -> Result<FilterChainRuntime>;

    [[nodiscard]] auto destroy() -> Result<void>;

    [[nodiscard]] auto load_preset(const std::filesystem::path& preset_path) -> Result<void>;
    [[nodiscard]] auto handle_resize(vk::Extent2D new_target_extent) -> Result<void>;
    [[nodiscard]] auto set_stage_policy(const ChainStagePolicy& policy) -> Result<void>;
    [[nodiscard]] auto get_stage_policy() const -> Result<ChainStagePolicy>;
    [[nodiscard]] auto set_prechain_resolution(vk::Extent2D resolution) -> Result<void>;
    [[nodiscard]] auto get_prechain_resolution() const -> Result<vk::Extent2D>;

    [[nodiscard]] auto record(const ChainRecordInfo& record_info) -> Result<void>;

    [[nodiscard]] auto list_controls() const -> Result<std::vector<ChainControlDescriptor>>;
    [[nodiscard]] auto list_controls(ChainControlStage stage) const
        -> Result<std::vector<ChainControlDescriptor>>;
    [[nodiscard]] auto set_control_value(std::uint64_t control_id, float value) -> Result<bool>;
    [[nodiscard]] auto reset_control_value(std::uint64_t control_id) -> Result<bool>;
    [[nodiscard]] auto reset_all_controls() -> Result<void>;

    [[nodiscard]] explicit operator bool() const { return m_handle != nullptr; }

private:
    explicit FilterChainRuntime(goggles_chain* handle) : m_handle(handle) {}

    goggles_chain* m_handle = nullptr;
};

} // namespace goggles::render

#undef GOGGLES_CHAIN_CPP_API
