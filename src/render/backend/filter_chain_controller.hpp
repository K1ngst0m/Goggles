#pragma once

#include "goggles_filter_chain.hpp"

#include <array>
#include <atomic>
#include <filesystem>
#include <functional>
#include <future>
#include <render/chain/filter_controls.hpp>
#include <render/chain/vulkan_context.hpp>
#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::render::backend_internal {

/// @brief Backend-side filter coordination state reserved for the future seam.
struct FilterChainController {
    static constexpr size_t MAX_DEFERRED_DESTROYS = 4;

    using BoundaryVulkanContext = goggles::render::VulkanContext;

    struct RuntimeBuildConfig {
        BoundaryVulkanContext vulkan_context;
        uint32_t graphics_queue_family_index = 0;
        vk::Format target_format = vk::Format::eUndefined;
        uint32_t num_sync_indices = 1;
        std::filesystem::path shader_dir;
        std::filesystem::path cache_dir;
        vk::Extent2D initial_prechain_resolution;
    };

    struct DeferredDestroy {
        FilterChainRuntime filter_chain;
        uint64_t destroy_after_frame = 0;
    };

    struct PrechainResolutionConfig {
        vk::Extent2D requested_resolution;
        vk::Extent2D fallback_resolution;
    };

    [[nodiscard]] auto recreate_filter_chain(const RuntimeBuildConfig& config) -> Result<void>;
    void shutdown(const std::function<void()>& wait_for_gpu_idle);

    void load_shader_preset(const std::filesystem::path& new_preset_path);
    [[nodiscard]] auto reload_shader_preset(std::filesystem::path new_preset_path,
                                            RuntimeBuildConfig config) -> Result<void>;

    void advance_frame();
    void check_pending_chain_swap(const std::function<void()>& wait_all_frames);
    void cleanup_deferred_destroys();

    void set_stage_policy(const ChainStagePolicy& policy);
    void set_prechain_resolution(const PrechainResolutionConfig& config);
    [[nodiscard]] auto handle_resize(vk::Extent2D target_extent) -> Result<void>;

    [[nodiscard]] auto resolve_initial_prechain_resolution(vk::Extent2D fallback_resolution) const
        -> vk::Extent2D;
    [[nodiscard]] auto current_prechain_resolution() const -> vk::Extent2D;
    [[nodiscard]] auto current_preset_path() const -> const std::filesystem::path& {
        return preset_path;
    }
    [[nodiscard]] auto has_filter_chain() const -> bool { return static_cast<bool>(filter_chain); }
    [[nodiscard]] auto filter_chain_runtime() -> FilterChainRuntime& { return filter_chain; }
    [[nodiscard]] auto consume_chain_swapped() -> bool {
        return chain_swapped.exchange(false, std::memory_order_acq_rel);
    }

    [[nodiscard]] auto list_filter_controls() const -> std::vector<FilterControlDescriptor>;
    [[nodiscard]] auto list_filter_controls(FilterControlStage stage) const
        -> std::vector<FilterControlDescriptor>;
    [[nodiscard]] auto set_filter_control_value(FilterControlId control_id, float value) -> bool;
    [[nodiscard]] auto reset_filter_control_value(FilterControlId control_id) -> bool;
    void reset_filter_controls();

    FilterChainRuntime filter_chain;
    FilterChainRuntime pending_filter_chain;
    std::filesystem::path preset_path;
    std::filesystem::path pending_preset_path;
    vk::Extent2D source_resolution;
    std::atomic<bool> pending_chain_ready{false};
    std::atomic<bool> chain_swapped{false};
    std::future<Result<void>> pending_load_future;
    std::array<DeferredDestroy, MAX_DEFERRED_DESTROYS> deferred_destroys{};
    size_t deferred_count = 0;
    uint64_t frame_count = 0;
    bool prechain_policy_enabled = true;
    bool effect_stage_policy_enabled = true;
};

} // namespace goggles::render::backend_internal
