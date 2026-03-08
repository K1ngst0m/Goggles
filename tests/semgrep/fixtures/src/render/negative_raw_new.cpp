#include <memory>

namespace goggles::render {

class Framebuffer final {};

auto build_framebuffer() -> std::unique_ptr<Framebuffer> {
    auto framebuffer = std::make_unique<Framebuffer>();
    return framebuffer;
}

} // namespace goggles::render
