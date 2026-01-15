#pragma once

#include "util/queues.hpp"
#include "vk_dispatch.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace goggles::capture {

enum class DumpFrameMode {
    ppm,
};

struct DumpRange {
    uint64_t begin = 0;
    uint64_t end = 0;
};

struct DumpSourceInfo {
    uint32_t stride = 0;
    uint32_t offset = 0;
    uint64_t modifier = 0;
};

struct TimelineWait {
    VkSemaphore semaphore = VK_NULL_HANDLE;
    uint64_t value = 0;
};

struct DumpJob {
    VkDevice device = VK_NULL_HANDLE;
    VkDeviceFuncs funcs{};

    VkFence fence = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    uint64_t frame_number = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;

    DumpSourceInfo src{};
    bool is_bgra = false;
    bool memory_is_coherent = true;
};

class FrameDumper {
public:
    /// @brief Manages best-effort asynchronous frame dumping for the Vulkan capture layer.
    FrameDumper();

    [[nodiscard]] auto is_enabled() const -> bool { return enabled_; }
    [[nodiscard]] auto has_pending() const -> bool { return !queue_.empty(); }

    /// @brief Schedules an async dump of an exportable swapchain image without waiting for GPU
    /// completion in the present call.
    /// @param queue Queue used for the transfer submission.
    /// @param dev_data Device dispatch table and state.
    /// @param image Image to copy from.
    /// @param width Image width in pixels.
    /// @param height Image height in pixels.
    /// @param format Image format.
    /// @param frame_number Monotonic frame identifier used for filenames.
    /// @param wait Timeline semaphore and value to wait for before copying.
    /// @param src Source metadata recorded alongside the dump.
    /// @return True if a dump job was enqueued for background draining; false otherwise.
    [[nodiscard]] auto try_schedule_export_image_dump(VkQueue queue, VkDeviceData* dev_data,
                                                      VkImage image, uint32_t width,
                                                      uint32_t height, VkFormat format,
                                                      uint64_t frame_number, TimelineWait wait,
                                                      const DumpSourceInfo& src) -> bool;

    /// @brief Schedules an async dump of a presented swapchain image without waiting for GPU
    /// completion in the present call.
    /// @param queue Queue used for the transfer submission.
    /// @param dev_data Device dispatch table and state.
    /// @param image Image to copy from.
    /// @param width Image width in pixels.
    /// @param height Image height in pixels.
    /// @param format Image format.
    /// @param frame_number Monotonic frame identifier used for filenames.
    /// @param src Source metadata recorded alongside the dump.
    /// @param wait_count Number of binary semaphores to wait on before copying.
    /// @param wait_semaphores Semaphores waited before copying (may be null when wait_count == 0).
    /// @return True if a dump job was enqueued for background draining; false otherwise.
    [[nodiscard]] auto try_schedule_present_image_dump(
        VkQueue queue, VkDeviceData* dev_data, VkImage image, uint32_t width, uint32_t height,
        VkFormat format, uint64_t frame_number, const DumpSourceInfo& src, uint32_t wait_count,
        const VkSemaphore* wait_semaphores) -> bool;

    /// @brief Drains queued dump jobs and writes outputs to disk.
    /// @note This may block waiting for GPU fences; do not call from `vkQueuePresentKHR`.
    void drain();

private:
    [[nodiscard]] auto should_dump_frame(uint64_t frame_number) const -> bool;

    void parse_env_config();

    [[nodiscard]] auto schedule_dump_copy(VkQueue queue, VkDeviceData* dev_data, VkImage image,
                                          uint32_t width, uint32_t height, VkFormat format,
                                          uint64_t frame_number, const DumpSourceInfo& src,
                                          uint32_t wait_count, const VkSemaphore* wait_semaphores)
        -> bool;

    [[nodiscard]] auto schedule_dump_copy_timeline(VkQueue queue, VkDeviceData* dev_data,
                                                   VkImage image, uint32_t width, uint32_t height,
                                                   VkFormat format, uint64_t frame_number,
                                                   const DumpSourceInfo& src, TimelineWait wait)
        -> bool;

    void drain_job(DumpJob& job);

    bool enabled_ = false;
    DumpFrameMode mode_ = DumpFrameMode::ppm;
    std::string dump_dir_ = "/tmp/goggles_dump";
    std::string process_name_ = "process";
    std::vector<DumpRange> ranges_;

    std::mutex queue_mutex_;
    util::SPSCQueue<DumpJob> queue_{64};
};

} // namespace goggles::capture
