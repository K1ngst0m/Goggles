namespace goggles::render {

class Framebuffer final {};

void destroy_framebuffers() {
    Framebuffer* framebuffers = nullptr;
    delete[] framebuffers;
}

} // namespace goggles::render
