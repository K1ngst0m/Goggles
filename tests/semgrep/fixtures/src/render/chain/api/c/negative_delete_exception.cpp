namespace goggles::render::chain::api::c {

class ApiBuffer final {};

void destroy_buffer() {
    ApiBuffer* buffer = nullptr;
    delete buffer;
}

} // namespace goggles::render::chain::api::c
