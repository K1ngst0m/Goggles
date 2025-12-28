#pragma once

#include "capture/capture_protocol.hpp"
#include "util/queues.hpp"
#include "vk_dispatch.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace goggles::capture {

struct CopyCmd {
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    uint64_t timeline_value = 0;
    bool busy = false;
};

struct AsyncCaptureItem {
    VkDevice device = VK_NULL_HANDLE;
    VkSemaphore timeline_sem = VK_NULL_HANDLE;
    uint64_t timeline_value = 0;
    int dmabuf_fd = -1;
    CaptureTextureData metadata{};
};

// Device-level sync state (independent of swapchain lifecycle)
struct DeviceSyncState {
    VkSemaphore frame_ready_sem = VK_NULL_HANDLE;
    VkSemaphore frame_consumed_sem = VK_NULL_HANDLE;
    int frame_ready_fd = -1;
    int frame_consumed_fd = -1;
    bool semaphores_sent = false;
    uint64_t frame_counter = 0;
    bool initialized = false;
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

    // Copy command buffers (one per swapchain image)
    std::vector<CopyCmd> copy_cmds;

    // State
    bool export_initialized = false;
    bool capture_active = false;
    bool dmabuf_sent = false;
};

class CaptureManager {
public:
    CaptureManager();
    ~CaptureManager();

    void on_swapchain_created(VkDevice device, VkSwapchainKHR swapchain,
                              const VkSwapchainCreateInfoKHR* create_info, VkDeviceData* dev_data);
    void on_swapchain_destroyed(VkDevice device, VkSwapchainKHR swapchain);
    void on_device_destroyed(VkDevice device, VkDeviceData* dev_data);
    void on_present(VkQueue queue, const VkPresentInfoKHR* present_info, VkDeviceData* dev_data);
    SwapData* get_swap_data(VkSwapchainKHR swapchain);
    void shutdown();

private:
    bool init_export_image(SwapData* swap, VkDeviceData* dev_data);
    bool init_device_sync(VkDevice device, VkDeviceData* dev_data);
    void reset_device_sync(VkDevice device, VkDeviceData* dev_data);
    void cleanup_device_sync(VkDevice device, VkDeviceData* dev_data);
    bool init_copy_cmds(SwapData* swap, VkDeviceData* dev_data);
    void destroy_copy_cmds(SwapData* swap, VkDeviceData* dev_data);
    void capture_frame(SwapData* swap, uint32_t image_index, VkQueue queue, VkDeviceData* dev_data,
                       VkPresentInfoKHR* present_info);
    void cleanup_swap_data(SwapData* swap, VkDeviceData* dev_data);

    DeviceSyncState* get_device_sync(VkDevice device);

    void worker_func();

    // Ordered for optimal padding (clang-analyzer-optin.performance.Padding)
    util::SPSCQueue<AsyncCaptureItem> async_queue_{16};
    std::thread worker_thread_;
    std::mutex mutex_;
    std::mutex cv_mutex_;
    std::condition_variable cv_;
    std::unordered_map<VkSwapchainKHR, SwapData> swaps_;
    std::unordered_map<VkDevice, DeviceSyncState> device_sync_;
    std::atomic<bool> shutdown_{false};
};

CaptureManager& get_capture_manager();

} // namespace goggles::capture
