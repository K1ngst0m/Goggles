#include <vulkan/vulkan.hpp>

namespace goggles::render {

void shutdown_device(vk::Device device) {
    device.waitIdle();
}

} // namespace goggles::render
