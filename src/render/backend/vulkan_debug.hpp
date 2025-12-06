#pragma once

#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

class VulkanDebugMessenger {
public:
    VulkanDebugMessenger() = default;
    ~VulkanDebugMessenger() = default;

    VulkanDebugMessenger(const VulkanDebugMessenger&) = delete;
    VulkanDebugMessenger& operator=(const VulkanDebugMessenger&) = delete;
    VulkanDebugMessenger(VulkanDebugMessenger&&) noexcept = default;
    VulkanDebugMessenger& operator=(VulkanDebugMessenger&&) noexcept = default;

    [[nodiscard]] static auto create(vk::Instance instance) -> Result<VulkanDebugMessenger>;

    [[nodiscard]] auto is_active() const -> bool { return static_cast<bool>(m_messenger); }

private:
    explicit VulkanDebugMessenger(vk::UniqueDebugUtilsMessengerEXT messenger);

    vk::UniqueDebugUtilsMessengerEXT m_messenger;
};

[[nodiscard]] auto is_validation_layer_available() -> bool;

} // namespace goggles::render
