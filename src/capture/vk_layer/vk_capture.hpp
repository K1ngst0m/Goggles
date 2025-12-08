#pragma once

#include "vk_dispatch.hpp"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace goggles::capture {

struct FrameData {
    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    bool cmd_buffer_busy = false;
};

struct SwapData {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    // Swapchain info
    VkExtent2D extent{};
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    // Swapchain images
    std::vector<VkImage> swap_images;

    // Export image for DMA-BUF
    VkImage export_image = VK_NULL_HANDLE;
    VkDeviceMemory export_mem = VK_NULL_HANDLE;

    // DMA-BUF info
    int dmabuf_fd = -1;
    uint32_t dmabuf_stride = 0;
    uint32_t dmabuf_offset = 0;
    uint64_t dmabuf_modifier = 0;

    // Per-frame resources
    std::vector<FrameData> frames;
    uint32_t frame_index = 0;

    // State
    bool export_initialized = false;
    bool capture_active = false;
};

class CaptureManager {
public:
    void on_swapchain_created(VkDevice device, VkSwapchainKHR swapchain,
                              const VkSwapchainCreateInfoKHR* create_info, VkDeviceData* dev_data);
    void on_swapchain_destroyed(VkDevice device, VkSwapchainKHR swapchain);
    void on_present(VkQueue queue, const VkPresentInfoKHR* present_info, VkDeviceData* dev_data);
    SwapData* get_swap_data(VkSwapchainKHR swapchain);

private:
    bool init_export_image(SwapData* swap, VkDeviceData* dev_data);
    void create_frame_resources(SwapData* swap, VkDeviceData* dev_data, uint32_t count);
    void destroy_frame_resources(SwapData* swap, VkDeviceData* dev_data);
    void capture_frame(SwapData* swap, uint32_t image_index, VkQueue queue, VkDeviceData* dev_data,
                       VkPresentInfoKHR* present_info);
    void cleanup_swap_data(SwapData* swap, VkDeviceData* dev_data);

    std::mutex mutex_;
    std::unordered_map<VkSwapchainKHR, SwapData> swaps_;
};

CaptureManager& get_capture_manager();

} // namespace goggles::capture
