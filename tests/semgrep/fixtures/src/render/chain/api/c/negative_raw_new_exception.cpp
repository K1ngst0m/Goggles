namespace goggles::render::chain::api::c {

class ApiBuffer final {};

auto build_buffer() -> ApiBuffer* {
    auto* buffer = new ApiBuffer();
    return buffer;
}

} // namespace goggles::render::chain::api::c
