#include <vulkan/vulkan.hpp>

namespace goggles::render {

void shutdown_device(vk::Device device) {
    static_cast<void>(device.waitIdle());
}

} // namespace goggles::render
