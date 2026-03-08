#include <vulkan/vulkan.h>

namespace goggles::capture::vk_layer {

struct LayerState {
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
};

} // namespace goggles::capture::vk_layer
