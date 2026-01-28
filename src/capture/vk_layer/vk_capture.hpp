#pragma once

#include "capture/capture_protocol.hpp"
#include "util/queues.hpp"
#include "vk_dispatch.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace goggles::capture {

class FrameDumper;

/// @brief Reusable copy command buffer state for exported swapchain images.
struct CopyCmd {
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    uint64_t timeline_value = 0;
    bool busy = false;
};

/// @brief Metadata for a virtual frame forwarded from a WSI proxy swapchain.
struct VirtualFrameInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint32_t stride = 0;
    uint32_t offset = 0;
    uint64_t modifier = 0;
    int dmabuf_fd = -1;
};

/// @brief Work item used by the capture worker thread.
struct AsyncCaptureItem {
    VkDevice device = VK_NULL_HANDLE;
    VkSemaphore timeline_sem = VK_NULL_HANDLE;
    uint64_t timeline_value = 0;
    int dmabuf_fd = -1;
    CaptureFrameMetadata metadata{};
};

/// @brief Device-level sync state independent of swapchain lifecycle.
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

/// @brief Snapshot of device sync state for lock-free reads.
struct DeviceSyncSnapshot {
    VkSemaphore frame_consumed_sem = VK_NULL_HANDLE;
    uint64_t frame_counter = 0;
    bool semaphores_sent = false;
    bool initialized = false;
};

/// @brief Per-swapchain capture bookkeeping and exported DMA-BUF resources.
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
    bool dmabuf_sent = false;
};

/// @brief Manages swapchain capture, DMA-BUF export, and IPC forwarding.
class CaptureManager {
public:
    CaptureManager();
    ~CaptureManager();

    /// @brief Tracks a newly created swapchain and initializes capture resources as needed.
    void on_swapchain_created(VkDevice device, VkSwapchainKHR swapchain,
                              const VkSwapchainCreateInfoKHR* create_info, VkDeviceData* dev_data);
    /// @brief Cleans up capture resources for a destroyed swapchain.
    void on_swapchain_destroyed(VkDevice device, VkSwapchainKHR swapchain);
    /// @brief Cleans up device-level capture state.
    void on_device_destroyed(VkDevice device, VkDeviceData* dev_data);
    /// @brief Handles a present call and schedules capture work if enabled.
    void on_present(VkQueue queue, const VkPresentInfoKHR* present_info, VkDeviceData* dev_data);
    /// @brief Returns swapchain capture state, or null if untracked.
    SwapData* get_swap_data(VkSwapchainKHR swapchain);
    /// @brief Returns a snapshot of device sync state.
    DeviceSyncSnapshot get_device_sync_snapshot(VkDevice device);
    /// @brief Ensures device-level sync primitives exist for capture.
    bool ensure_device_sync(VkDevice device, VkDeviceData* dev_data);
    /// @brief Attempts to send device sync semaphore FDs to the receiver.
    bool try_send_device_semaphores(VkDevice device);
    /// @brief Returns a monotonically increasing counter for virtual frames.
    uint64_t get_virtual_frame_counter() const;
    /// @brief Stops background work and disconnects any IPC state.
    void shutdown();

    /// @brief Queues a virtual frame (from WSI proxy) for forwarding.
    uint64_t enqueue_virtual_frame(const VirtualFrameInfo& frame);

    /// @brief Schedules a present-image dump for WSI proxy mode when enabled.
    void try_dump_present_image(VkQueue queue, const VkPresentInfoKHR* present_info, VkImage image,
                                uint32_t width, uint32_t height, VkFormat format,
                                const VirtualFrameInfo& frame, uint64_t frame_number,
                                VkDeviceData* dev_data);

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
    std::atomic<uint64_t> virtual_frame_counter_{0};
    std::unique_ptr<FrameDumper> frame_dumper_;
};

/// @brief Returns the process-wide capture manager instance.
CaptureManager& get_capture_manager();

} // namespace goggles::capture
