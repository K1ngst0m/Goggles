#include <memory>

namespace goggles::render {

class Framebuffer final {};

void destroy_framebuffer() {
    auto framebuffer = std::make_unique<Framebuffer>();
    framebuffer.reset();
}

} // namespace goggles::render
