#include <vulkan/vulkan.h>

namespace goggles::render::chain {

struct SwapImage {
    VkImage image = VK_NULL_HANDLE;
};

} // namespace goggles::render::chain
