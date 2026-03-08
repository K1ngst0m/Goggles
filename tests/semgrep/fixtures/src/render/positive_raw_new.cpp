namespace goggles::render {

class Framebuffer final {};

auto build_framebuffer() -> Framebuffer* {
    Framebuffer* framebuffer = new Framebuffer{};
    return framebuffer;
}

} // namespace goggles::render
