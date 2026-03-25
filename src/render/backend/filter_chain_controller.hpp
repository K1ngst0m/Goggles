#pragma once

#include <array>
#include <atomic>
#include <filesystem>
#include <functional>
#include <future>
#include <goggles/filter_chain.h>
#include <goggles/filter_chain.hpp>
#include <goggles/filter_chain/filter_controls.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

namespace goggles::render::backend_internal {

struct FilterChainController {
    struct ControlOverride {
        goggles::fc::FilterControlId control_id = 0;
        float value = 0.0F;
    };

    struct VulkanDeviceInfo {
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue graphics_queue = VK_NULL_HANDLE;
        uint32_t graphics_queue_family_index = 0;
        std::string cache_dir;
    };

    struct ChainConfig {
        VkFormat target_format = VK_FORMAT_UNDEFINED;
        uint32_t frames_in_flight = 1;
        uint32_t initial_stage_mask = GOGGLES_FC_STAGE_MASK_ALL;
        uint32_t initial_prechain_width = 0;
        uint32_t initial_prechain_height = 0;
    };

    struct RecordParams {
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VkImage source_image = VK_NULL_HANDLE;
        VkImageView source_view = VK_NULL_HANDLE;
        uint32_t source_width = 0;
        uint32_t source_height = 0;
        VkImageView target_view = VK_NULL_HANDLE;
        uint32_t target_width = 0;
        uint32_t target_height = 0;
        uint32_t frame_index = 0;
        uint32_t scale_mode = GOGGLES_FC_SCALE_MODE_STRETCH;
        uint32_t integer_scale = 1;
    };

    struct OutputTarget {
        vk::Format format = vk::Format::eUndefined;
        vk::Extent2D extent;
    };

    [[nodiscard]] auto recreate_filter_chain(const VulkanDeviceInfo& device_info,
                                             ChainConfig chain_config) -> Result<void>;
    [[nodiscard]] auto retarget_filter_chain(const OutputTarget& output_target) -> Result<void>;
    void shutdown(const std::function<void()>& wait_for_gpu_idle);

    void load_shader_preset(
        const std::filesystem::path& new_preset_path,
        const std::function<void()>& wait_for_safe_rebuild = std::function<void()>{});
    [[nodiscard]] auto reload_shader_preset(std::filesystem::path new_preset_path,
                                            const VulkanDeviceInfo& device_info,
                                            ChainConfig chain_config) -> Result<void>;

    void advance_frame();
    void check_pending_chain_swap(const std::function<void()>& wait_all_frames);
    void cleanup_retired_adapters();

    void
    set_stage_policy(bool prechain_enabled, bool effect_stage_enabled,
                     const std::function<void()>& wait_for_safe_rebuild = std::function<void()>{});
    void set_prechain_resolution(
        vk::Extent2D resolution,
        const std::function<void()>& wait_for_safe_rebuild = std::function<void()>{});
    [[nodiscard]] auto handle_resize(vk::Extent2D target_extent) -> Result<void>;
    [[nodiscard]] auto record(const RecordParams& record_params) -> Result<void>;

    [[nodiscard]] auto current_prechain_resolution() const -> vk::Extent2D;
    [[nodiscard]] auto current_preset_path() const -> const std::filesystem::path& {
        return preset_path;
    }
    [[nodiscard]] auto has_filter_chain() const -> bool {
        return static_cast<bool>(active_slot.chain);
    }
    [[nodiscard]] auto consume_chain_swapped() -> bool {
        return chain_swapped.exchange(false, std::memory_order_acq_rel);
    }

    [[nodiscard]] auto get_chain_report() const -> goggles::Result<goggles_fc_chain_report_t>;

    [[nodiscard]] auto list_filter_controls() const
        -> std::vector<goggles::fc::FilterControlDescriptor>;
    [[nodiscard]] auto list_filter_controls(goggles::fc::FilterControlStage stage) const
        -> std::vector<goggles::fc::FilterControlDescriptor>;
    [[nodiscard]] auto set_filter_control_value(goggles::fc::FilterControlId control_id,
                                                float value) -> bool;
    [[nodiscard]] auto reset_filter_control_value(goggles::fc::FilterControlId control_id) -> bool;
    void reset_filter_controls();

    struct FilterChainSlot {
        goggles::filter_chain::Instance instance;
        goggles::filter_chain::Device device;
        goggles::filter_chain::Program program;
        goggles::filter_chain::Chain chain;

        VkFormat target_format = VK_FORMAT_UNDEFINED;
        uint32_t frames_in_flight = 1;
        uint32_t stage_mask = GOGGLES_FC_STAGE_MASK_ALL;
        uint32_t prechain_width = 0;
        uint32_t prechain_height = 0;
    };

    struct RetiredAdapter {
        FilterChainSlot slot;
        uint64_t destroy_after_frame = 0;
    };

    struct RetiredAdapterTracker {
        static constexpr size_t MAX_RETIRED = 4;
        static constexpr uint64_t FALLBACK_RETIRE_DELAY_FRAMES = 3;

        std::array<RetiredAdapter, MAX_RETIRED> retired{};
        size_t retired_count = 0;
    };

    FilterChainSlot active_slot;
    FilterChainSlot pending_slot;
    std::filesystem::path preset_path;
    std::filesystem::path pending_preset_path;
    vk::Extent2D source_resolution;
    std::atomic<bool> pending_chain_ready{false};
    std::atomic<bool> chain_swapped{false};
    std::future<Result<void>> pending_load_future;
    RetiredAdapterTracker retired_adapters;
    std::vector<ControlOverride> authoritative_control_overrides;
    uint64_t frame_count = 0;
    bool prechain_policy_enabled = true;
    bool effect_stage_policy_enabled = true;
    OutputTarget authoritative_output_target;
};

} // namespace goggles::render::backend_internal
