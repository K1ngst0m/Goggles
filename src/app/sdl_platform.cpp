#include "sdl_platform.hpp"

#include <SDL3/SDL.h>
#include <cstdint>
#include <memory>
#include <string>

namespace goggles::app {

static auto to_sdl_window(void* window_handle) -> SDL_Window* {
    return static_cast<SDL_Window*>(window_handle);
}

auto SdlPlatform::create(const CreateInfo& create_info) -> ResultPtr<SdlPlatform> {
    auto platform = std::unique_ptr<SdlPlatform>(new SdlPlatform());

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return make_result_ptr_error<SdlPlatform>(
            ErrorCode::unknown_error, "Failed to initialize SDL3: " + std::string(SDL_GetError()));
    }
    platform->m_sdl_initialized = true;

    auto window_flags =
        static_cast<SDL_WindowFlags>((create_info.enable_vulkan ? SDL_WINDOW_VULKAN : 0) |
                                     (create_info.resizable ? SDL_WINDOW_RESIZABLE : 0));

    auto* window = SDL_CreateWindow(create_info.title.c_str(), create_info.width,
                                    create_info.height, window_flags);
    if (window == nullptr) {
        return make_result_ptr_error<SdlPlatform>(
            ErrorCode::unknown_error, "Failed to create window: " + std::string(SDL_GetError()));
    }
    platform->m_window = window;

    return make_result_ptr(std::move(platform));
}

SdlPlatform::~SdlPlatform() {
    shutdown();
}

void SdlPlatform::shutdown() {
    if (m_window != nullptr) {
        SDL_DestroyWindow(to_sdl_window(m_window));
        m_window = nullptr;
    }
    if (m_sdl_initialized) {
        SDL_Quit();
        m_sdl_initialized = false;
    }
}

} // namespace goggles::app
