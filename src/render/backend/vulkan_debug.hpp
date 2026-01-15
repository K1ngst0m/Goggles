#pragma once

#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

/// @brief RAII wrapper for Vulkan debug utils messenger.
class VulkanDebugMessenger {
public:
    VulkanDebugMessenger() = default;
    ~VulkanDebugMessenger() = default;

    VulkanDebugMessenger(const VulkanDebugMessenger&) = delete;
    VulkanDebugMessenger& operator=(const VulkanDebugMessenger&) = delete;
    VulkanDebugMessenger(VulkanDebugMessenger&&) noexcept = default;
    VulkanDebugMessenger& operator=(VulkanDebugMessenger&&) noexcept = default;

    /// @brief Creates a debug messenger for `instance`.
    /// @return An active messenger or an error.
    [[nodiscard]] static auto create(vk::Instance instance) -> Result<VulkanDebugMessenger>;

    /// @brief Returns true if the messenger was created.
    [[nodiscard]] auto is_active() const -> bool { return static_cast<bool>(m_messenger); }

private:
    explicit VulkanDebugMessenger(vk::UniqueDebugUtilsMessengerEXT messenger);

    vk::UniqueDebugUtilsMessengerEXT m_messenger;
};

/// @brief Returns true if Vulkan validation layers appear to be available.
[[nodiscard]] auto is_validation_layer_available() -> bool;

} // namespace goggles::render
