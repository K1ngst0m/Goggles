#pragma once

#include "vulkan_context.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <util/error.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace goggles::render::backend_internal {

/// @brief Backend-owned presentation and headless target state.
struct RenderOutput {
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    struct FrameResources {
        vk::CommandBuffer command_buffer;
        vk::Fence in_flight_fence;
        vk::Semaphore image_available_sem;
    };

    [[nodiscard]] auto create_swapchain(VulkanContext& context, uint32_t width, uint32_t height,
                                        vk::Format preferred_format) -> Result<void>;
    void cleanup_swapchain(VulkanContext& context);
    [[nodiscard]] auto create_command_resources(VulkanContext& context) -> Result<void>;
    [[nodiscard]] auto create_sync_objects(VulkanContext& context) -> Result<void>;
    [[nodiscard]] auto create_sync_objects_headless(VulkanContext& context) -> Result<void>;
    [[nodiscard]] auto create_offscreen_image(VulkanContext& context,
                                              vk::Extent2D source_resolution) -> Result<void>;
    void destroy(VulkanContext& context);
    void wait_all_frames(VulkanContext& context);

    [[nodiscard]] auto acquire_next_image(VulkanContext& context) -> Result<uint32_t>;
    [[nodiscard]] auto prepare_headless_frame(VulkanContext& context) -> Result<vk::CommandBuffer>;
    [[nodiscard]] auto submit_and_present(VulkanContext& context, uint32_t image_index,
                                          vk::Semaphore acquire_wait_semaphore = nullptr,
                                          vk::PipelineStageFlags acquire_wait_stage =
                                              vk::PipelineStageFlagBits::eColorAttachmentOutput)
        -> Result<void>;
    [[nodiscard]] auto submit_headless(VulkanContext& context,
                                       vk::Semaphore acquire_wait_semaphore = nullptr,
                                       vk::PipelineStageFlags acquire_wait_stage =
                                           vk::PipelineStageFlagBits::eColorAttachmentOutput)
        -> Result<void>;
    [[nodiscard]] auto readback_to_png(VulkanContext& context, const std::filesystem::path& output)
        -> Result<void>;

    void set_target_fps(uint32_t value) {
        target_fps = value;
        last_present_time = std::chrono::steady_clock::time_point{};
    }

    [[nodiscard]] auto command_buffer() const -> vk::CommandBuffer {
        return frames[current_frame].command_buffer;
    }

    [[nodiscard]] auto headless_command_buffer() const -> vk::CommandBuffer {
        return frames[0].command_buffer;
    }

    [[nodiscard]] auto current_frame_slot() const -> uint32_t { return current_frame; }
    [[nodiscard]] auto is_headless() const -> bool { return headless; }
    [[nodiscard]] auto target_extent() const -> vk::Extent2D {
        return headless ? offscreen_extent : swapchain_extent;
    }
    [[nodiscard]] auto target_image(uint32_t image_index = 0) const -> vk::Image {
        return headless ? offscreen_image : swapchain_images[image_index];
    }
    [[nodiscard]] auto target_view(uint32_t image_index = 0) const -> vk::ImageView {
        return headless ? offscreen_view : swapchain_image_views[image_index];
    }
    [[nodiscard]] auto image_count() const -> uint32_t {
        return static_cast<uint32_t>(swapchain_images.size());
    }
    void clear_resize_request() { needs_resize = false; }

    vk::SwapchainKHR swapchain;
    vk::CommandPool command_pool;
    std::vector<vk::Image> swapchain_images;
    std::vector<vk::ImageView> swapchain_image_views;
    std::vector<vk::Semaphore> render_finished_sems;
    std::array<FrameResources, MAX_FRAMES_IN_FLIGHT> frames{};

    vk::Image offscreen_image;
    vk::DeviceMemory offscreen_memory;
    vk::ImageView offscreen_view;
    vk::Extent2D offscreen_extent;

    vk::Format swapchain_format = vk::Format::eUndefined;
    vk::Extent2D swapchain_extent;
    uint32_t current_frame = 0;
    bool headless = false;
    bool needs_resize = false;
    uint32_t target_fps = 0;
    uint64_t present_id = 0;
    std::chrono::steady_clock::time_point last_present_time;
};

} // namespace goggles::render::backend_internal
