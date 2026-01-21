#include "vulkan_debug.hpp"

#include <algorithm>
#include <cstring>
#include <util/logging.hpp>

namespace goggles::render {

namespace {

constexpr const char* VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

VKAPI_ATTR auto VKAPI_CALL
debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
               [[maybe_unused]] vk::DebugUtilsMessageTypeFlagsEXT message_type,
               const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
               [[maybe_unused]] void* user_data) -> VkBool32 {

    const char* message = callback_data->pMessage != nullptr ? callback_data->pMessage : "";

    if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
        GOGGLES_LOG_ERROR("[VVL] {}", message);
    } else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        GOGGLES_LOG_WARN("[VVL] {}", message);
    } else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
        GOGGLES_LOG_DEBUG("[VVL] {}", message);
    } else {
        GOGGLES_LOG_TRACE("[VVL] {}", message);
    }

    return VK_FALSE;
}

} // namespace

VulkanDebugMessenger::VulkanDebugMessenger(vk::UniqueDebugUtilsMessengerEXT messenger)
    : m_messenger(std::move(messenger)) {}

auto VulkanDebugMessenger::create(vk::Instance instance) -> Result<VulkanDebugMessenger> {
    vk::DebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
    create_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                              vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                              vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    create_info.pfnUserCallback = debug_callback;
    create_info.pUserData = nullptr;

    auto [result, messenger] = instance.createDebugUtilsMessengerEXTUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<VulkanDebugMessenger>(ErrorCode::vulkan_init_failed,
                                                "Failed to create debug messenger: " +
                                                    vk::to_string(result));
    }

    GOGGLES_LOG_DEBUG("Vulkan debug messenger created");
    return VulkanDebugMessenger(std::move(messenger));
}

auto is_validation_layer_available() -> bool {
    auto [result, layers] = vk::enumerateInstanceLayerProperties();
    if (result != vk::Result::eSuccess) {
        return false;
    }

    return std::ranges::any_of(layers, [](const vk::LayerProperties& props) {
        return std::strcmp(props.layerName.data(), VALIDATION_LAYER_NAME) == 0;
    });
}

} // namespace goggles::render
