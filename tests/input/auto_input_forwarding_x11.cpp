#include "compositor/compositor_server.hpp"

#include <SDL3/SDL.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <vector>

namespace {

auto has_xwayland() -> bool {
    // NOLINTNEXTLINE(concurrency-mt-unsafe): test-only environment check
    return std::system("sh -lc 'command -v Xwayland >/dev/null 2>&1'") == 0;
}

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
    bool initialized = false;

    ~SDLGuard() {
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
    if (!has_xwayland()) {
        std::fprintf(stderr, "[SKIP] Xwayland not found on PATH\n");
        return 0;
    }

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

    auto display = compositor->x11_display();
    if (display.empty()) {
        std::fprintf(stderr, "[FAIL] CompositorServer returned empty DISPLAY\n");
        return 1;
    }

    setenv("DISPLAY", display.c_str(), 1);
    setenv("SDL_VIDEODRIVER", "x11", 1);

    SDLGuard sdl{};
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "[FAIL] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    sdl.initialized = true;

    sdl.window =
        SDL_CreateWindow("Goggles Auto Input Forwarding (X11)", 640, 360, SDL_WINDOW_RESIZABLE);
    if (!sdl.window) {
        std::fprintf(stderr, "[FAIL] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_ShowWindow(sdl.window);
    SDL_RaiseWindow(sdl.window);

    // Give XWayland/wlroots a moment to finish mapping/association.
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
            stderr, "[FAIL] No SDL_EVENT_KEY_DOWN received (X11 input forwarding likely broken)\n");
        return 1;
    }

    std::fprintf(stderr, "[OK] Received SDL_EVENT_KEY_DOWN via XWayland\n");
    return 0;
}
