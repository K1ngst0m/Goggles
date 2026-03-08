#pragma once

#include "vulkan_debug.hpp"

#include <cstdint>
#include <optional>
#include <render/chain/vulkan_context.hpp>
#include <string>
#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

struct SDL_Window;

namespace goggles::render::backend_internal {

/// @brief Backend-owned Vulkan root state for the future subsystem split.
struct VulkanContext {
    VulkanContext() = default;
    ~VulkanContext() = default;

    VulkanContext(const VulkanContext&) = delete;
    auto operator=(const VulkanContext&) -> VulkanContext& = delete;

    VulkanContext(VulkanContext&& other) noexcept;
    auto operator=(VulkanContext&& other) noexcept -> VulkanContext&;

    [[nodiscard]] static auto create(SDL_Window* window, bool enable_validation,
                                     const std::string& gpu_selector) -> Result<VulkanContext>;
    [[nodiscard]] static auto create_headless(bool enable_validation,
                                              const std::string& gpu_selector)
        -> Result<VulkanContext>;

    void destroy();

    [[nodiscard]] auto boundary_context(vk::CommandPool command_pool) const
        -> ::goggles::render::VulkanContext;
    [[nodiscard]] auto initialized() const -> bool { return static_cast<bool>(device); }
    [[nodiscard]] auto is_headless() const -> bool { return headless; }

    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;
    vk::Queue graphics_queue;
    vk::SurfaceKHR surface;
    std::optional<VulkanDebugMessenger> debug_messenger;
    uint32_t graphics_queue_family = UINT32_MAX;
    uint32_t gpu_index = 0;
    std::string gpu_uuid;
    bool enable_validation = false;
    bool headless = false;
    bool present_wait_supported = false;
};

} // namespace goggles::render::backend_internal
