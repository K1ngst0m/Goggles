#pragma once

#include <goggles/error.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

class VulkanDebugMessenger {
public:
    VulkanDebugMessenger() = default;
    ~VulkanDebugMessenger();

    VulkanDebugMessenger(const VulkanDebugMessenger&) = delete;
    VulkanDebugMessenger& operator=(const VulkanDebugMessenger&) = delete;
    VulkanDebugMessenger(VulkanDebugMessenger&& other) noexcept;
    VulkanDebugMessenger& operator=(VulkanDebugMessenger&& other) noexcept;

    [[nodiscard]] static auto create(vk::Instance instance) -> Result<VulkanDebugMessenger>;
    [[nodiscard]] auto is_active() const -> bool { return static_cast<bool>(m_messenger); }

private:
    VulkanDebugMessenger(vk::Instance instance, vk::DebugUtilsMessengerEXT messenger);
    void reset();

    vk::Instance m_instance;
    vk::DebugUtilsMessengerEXT m_messenger;
};

[[nodiscard]] auto is_validation_layer_available() -> bool;

} // namespace goggles::render
