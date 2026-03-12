#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

/// @brief Vulkan objects shared by render passes.
struct VulkanContext {
    vk::Device device;
    vk::PhysicalDevice physical_device;
    vk::CommandPool command_pool;
    vk::Queue graphics_queue;
    uint32_t graphics_queue_family_index = UINT32_MAX;
};

} // namespace goggles::render
