#pragma once

#include "vk_dispatch.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace goggles::capture {

struct VirtualSurface {
    VkSurfaceKHR handle = VK_NULL_HANDLE;
    VkInstance instance = VK_NULL_HANDLE;
    uint32_t width = 1920;
    uint32_t height = 1080;
};

struct VirtualSwapchain {
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkDeviceMemory> memory;
    std::vector<int> dmabuf_fds;
    std::vector<uint32_t> strides;
    std::vector<uint32_t> offsets;
    std::vector<uint64_t> modifiers;
    uint32_t image_count = 0;
    uint32_t current_index = 0;
    std::chrono::steady_clock::time_point last_acquire;
};

struct SwapchainFrameData {
    bool valid = false;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t stride = 0;
    uint32_t offset = 0;
    uint64_t modifier = 0;
    int dmabuf_fd = -1;
};

class WsiVirtualizer {
public:
    static WsiVirtualizer& instance();

    bool is_enabled() const { return enabled_; }

    VkResult create_surface(VkInstance inst, VkSurfaceKHR* surface);
    void destroy_surface(VkInstance inst, VkSurfaceKHR surface);
    bool is_virtual_surface(VkSurfaceKHR surface);
    VirtualSurface* get_surface(VkSurfaceKHR surface);

    VkResult get_surface_capabilities(VkPhysicalDevice phys_dev, VkSurfaceKHR surface,
                                      VkSurfaceCapabilitiesKHR* caps);
    VkResult get_surface_formats(VkPhysicalDevice phys_dev, VkSurfaceKHR surface, uint32_t* count,
                                 VkSurfaceFormatKHR* formats);
    VkResult get_surface_present_modes(VkPhysicalDevice phys_dev, VkSurfaceKHR surface,
                                       uint32_t* count, VkPresentModeKHR* modes);
    VkResult get_surface_support(VkPhysicalDevice phys_dev, uint32_t queue_family,
                                 VkSurfaceKHR surface, VkBool32* supported, VkInstData* inst_data);

    VkResult create_swapchain(VkDevice device, const VkSwapchainCreateInfoKHR* info,
                              VkSwapchainKHR* swapchain, VkDeviceData* dev_data);
    void destroy_swapchain(VkDevice device, VkSwapchainKHR swapchain, VkDeviceData* dev_data);
    bool is_virtual_swapchain(VkSwapchainKHR swapchain);
    VirtualSwapchain* get_swapchain(VkSwapchainKHR swapchain);

    VkResult get_swapchain_images(VkSwapchainKHR swapchain, uint32_t* count, VkImage* images);
    VkResult acquire_next_image(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout,
                                VkSemaphore semaphore, VkFence fence, uint32_t* index,
                                VkDeviceData* dev_data);
    SwapchainFrameData get_frame_data(VkSwapchainKHR swapchain, uint32_t image_index);

private:
    WsiVirtualizer();
    ~WsiVirtualizer();

    VkSurfaceKHR generate_surface_handle();
    VkSwapchainKHR generate_swapchain_handle();
    bool create_exportable_images(VirtualSwapchain& swap, VkDevice device, VkDeviceData* dev_data);
    void destroy_swapchain_resources(VirtualSwapchain& swap, VkDevice device,
                                     VkDeviceData* dev_data);

    bool enabled_ = false;
    std::mutex mutex_;
    std::unordered_map<VkSurfaceKHR, VirtualSurface> surfaces_;
    std::unordered_map<VkSwapchainKHR, VirtualSwapchain> swapchains_;
    uint64_t next_handle_ = 0x7000000000000000ULL;
};

bool should_use_wsi_proxy();

} // namespace goggles::capture
