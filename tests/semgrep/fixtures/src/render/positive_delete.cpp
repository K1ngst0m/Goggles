namespace goggles::render {

class Framebuffer final {};

struct FramebufferOwner final {
    Framebuffer* framebuffer;
};

void destroy_framebuffer() {
    FramebufferOwner owner{.framebuffer = nullptr};
    delete owner.framebuffer;
}

} // namespace goggles::render
