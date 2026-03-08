namespace goggles::render {

class Framebuffer final {};

void destroy_framebuffer() {
    Framebuffer* framebuffer = nullptr;
    delete framebuffer;
}

} // namespace goggles::render
