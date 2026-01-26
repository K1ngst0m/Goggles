#include "compositor/compositor_server.hpp"

#include <SDL3/SDL.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <vector>

namespace {

auto make_xdg_runtime_dir() -> std::filesystem::path {
    std::filesystem::path base = std::filesystem::temp_directory_path() / "goggles-xdg-runtime";
    std::filesystem::create_directories(base);

    std::string tmpl = (base / "XXXXXX").string();
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    char* dir = mkdtemp(buf.data());
    if (!dir) {
        std::perror("mkdtemp");
        return {};
    }

    std::filesystem::permissions(dir, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    return dir;
}

struct SDLGuard {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool initialized = false;

    ~SDLGuard() {
        if (renderer) {
            SDL_DestroyRenderer(renderer);
        }
        if (window) {
            SDL_DestroyWindow(window);
        }
        if (initialized) {
            SDL_Quit();
        }
    }
};

} // namespace

auto main() -> int {
    auto runtime_dir = make_xdg_runtime_dir();
    if (runtime_dir.empty()) {
        std::fprintf(stderr, "[FAIL] Failed to create XDG_RUNTIME_DIR\n");
        return 1;
    }

    setenv("XDG_RUNTIME_DIR", runtime_dir.c_str(), 1);

    auto compositor_result = goggles::input::CompositorServer::create();
    if (!compositor_result) {
        std::fprintf(stderr, "[FAIL] CompositorServer::create failed: %s\n",
                     compositor_result.error().message.c_str());
        return 1;
    }
    auto compositor = std::move(compositor_result.value());

    auto wayland_display = compositor->wayland_display();
    if (wayland_display.empty()) {
        std::fprintf(stderr, "[FAIL] CompositorServer returned empty WAYLAND_DISPLAY\n");
        return 1;
    }

    setenv("WAYLAND_DISPLAY", wayland_display.c_str(), 1);
    setenv("SDL_VIDEODRIVER", "wayland", 1);

    SDLGuard sdl{};
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "[SKIP] SDL_Init (wayland) failed: %s\n", SDL_GetError());
        return 0;
    }
    sdl.initialized = true;

    sdl.window =
        SDL_CreateWindow("Goggles Auto Input Forwarding (Wayland)", 640, 360, SDL_WINDOW_RESIZABLE);
    if (!sdl.window) {
        std::fprintf(stderr, "[SKIP] SDL_CreateWindow (wayland) failed: %s\n", SDL_GetError());
        return 0;
    }
    SDL_ShowWindow(sdl.window);

    // Wayland clients often don't become "mapped" (and won't receive input) until they commit a
    // buffer. Create a renderer and present once to force an initial commit.
    sdl.renderer = SDL_CreateRenderer(sdl.window, nullptr);
    if (!sdl.renderer) {
        std::fprintf(stderr, "[SKIP] SDL_CreateRenderer (wayland) failed: %s\n", SDL_GetError());
        return 0;
    }
    SDL_SetRenderDrawColor(sdl.renderer, 0, 40, 0, 255);
    SDL_RenderClear(sdl.renderer);
    SDL_RenderPresent(sdl.renderer);

    // Give wlroots/xdg-shell time to create/map and focus the surface.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    bool saw_key_down = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    while (!saw_key_down && std::chrono::steady_clock::now() < deadline) {
        SDL_KeyboardEvent key{};
        key.scancode = SDL_SCANCODE_A;
        key.down = true;
        (void)compositor->forward_key(key);
        key.down = false;
        (void)compositor->forward_key(key);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_A) {
                saw_key_down = true;
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    if (!saw_key_down) {
        std::fprintf(
            stderr,
            "[FAIL] No SDL_EVENT_KEY_DOWN received (Wayland input forwarding likely broken)\n");
        return 1;
    }

    std::fprintf(stderr, "[OK] Received SDL_EVENT_KEY_DOWN via Wayland\n");
    return 0;
}
