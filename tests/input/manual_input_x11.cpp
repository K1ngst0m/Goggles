#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <vulkan/vulkan.h>

namespace {

struct VulkanInstance {
    VkInstance handle = VK_NULL_HANDLE;

    VulkanInstance() = default;

    ~VulkanInstance() {
        if (handle) {
            vkDestroyInstance(handle, nullptr);
        }
    }

    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;

    VulkanInstance(VulkanInstance&& other) noexcept : handle(other.handle) {
        other.handle = VK_NULL_HANDLE;
    }

    VulkanInstance& operator=(VulkanInstance&& other) noexcept {
        if (this != &other) {
            if (handle) {
                vkDestroyInstance(handle, nullptr);
            }
            handle = other.handle;
            other.handle = VK_NULL_HANDLE;
        }
        return *this;
    }
};

struct SDLContext {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool initialized = false;

    SDLContext() = default;

    ~SDLContext() {
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

    SDLContext(const SDLContext&) = delete;
    SDLContext& operator=(const SDLContext&) = delete;

    SDLContext(SDLContext&& other) noexcept
        : window(other.window), renderer(other.renderer), initialized(other.initialized) {
        other.window = nullptr;
        other.renderer = nullptr;
        other.initialized = false;
    }

    SDLContext& operator=(SDLContext&& other) noexcept {
        if (this != &other) {
            if (renderer) {
                SDL_DestroyRenderer(renderer);
            }
            if (window) {
                SDL_DestroyWindow(window);
            }
            if (initialized) {
                SDL_Quit();
            }

            window = other.window;
            renderer = other.renderer;
            initialized = other.initialized;

            other.window = nullptr;
            other.renderer = nullptr;
            other.initialized = false;
        }
        return *this;
    }
};

} // namespace

static auto init_vulkan() -> VulkanInstance {
    VulkanInstance vk;

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Goggles Input Test (X11)";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    VkResult result = vkCreateInstance(&create_info, nullptr, &vk.handle);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "[goggles_manual_input_x11] vkCreateInstance failed: %d\n", result);
        std::exit(1);
    }

    return vk;
}

static auto init_sdl() -> SDLContext {
    SDLContext sdl;

    setenv("SDL_VIDEODRIVER", "x11", 1);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "[goggles_manual_input_x11] SDL_Init failed: %s\n", SDL_GetError());
        std::exit(1);
    }

    sdl.initialized = true;

    sdl.window = SDL_CreateWindow("Goggles Manual Input (X11)", 1280, 720, SDL_WINDOW_RESIZABLE);
    if (!sdl.window) {
        fprintf(stderr, "[goggles_manual_input_x11] SDL_CreateWindow failed: %s\n", SDL_GetError());
        std::exit(1);
    }

    sdl.renderer = SDL_CreateRenderer(sdl.window, nullptr);
    if (!sdl.renderer) {
        fprintf(stderr, "[goggles_manual_input_x11] SDL_CreateRenderer failed: %s\n",
                SDL_GetError());
        std::exit(1);
    }

    SDL_SetRenderDrawColor(sdl.renderer, 40, 0, 0, 255);
    SDL_RenderClear(sdl.renderer);
    SDL_RenderPresent(sdl.renderer);

    return sdl;
}

static void toggle_pointer_lock(SDL_Window* window) {
    bool current = SDL_GetWindowRelativeMouseMode(window);
    SDL_SetWindowRelativeMouseMode(window, !current);
    printf("[Mode] Pointer lock: %s\n", !current ? "ON" : "OFF");
    fflush(stdout);
}

static void toggle_mouse_grab(SDL_Window* window) {
    bool current = SDL_GetWindowMouseGrab(window);
    SDL_SetWindowMouseGrab(window, !current);
    printf("[Mode] Mouse grab: %s\n", !current ? "ON" : "OFF");
    fflush(stdout);
}

static void print_state(SDL_Window* window) {
    printf("[State] Pointer lock: %s, Mouse grab: %s\n",
           SDL_GetWindowRelativeMouseMode(window) ? "ON" : "OFF",
           SDL_GetWindowMouseGrab(window) ? "ON" : "OFF");
    fflush(stdout);
}

static void print_event_info(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
        printf("[Input] KEY DOWN: scancode=%d keycode=%d name='%s'\n", event.key.scancode,
               event.key.key, SDL_GetScancodeName(event.key.scancode));
        break;

    case SDL_EVENT_KEY_UP:
        printf("[Input] KEY UP: scancode=%d keycode=%d name='%s'\n", event.key.scancode,
               event.key.key, SDL_GetScancodeName(event.key.scancode));
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        printf("[Input] MOUSE BUTTON DOWN: button=%d at (%f, %f)\n", event.button.button,
               event.button.x, event.button.y);
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        printf("[Input] MOUSE BUTTON UP: button=%d at (%f, %f)\n", event.button.button,
               event.button.x, event.button.y);
        break;

    case SDL_EVENT_MOUSE_MOTION:
        printf("[Input] MOUSE MOTION: position=(%f, %f) relative=(%f, %f)\n", event.motion.x,
               event.motion.y, event.motion.xrel, event.motion.yrel);
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        printf("[Input] MOUSE WHEEL: scroll=(%f, %f)\n", event.wheel.x, event.wheel.y);
        break;
    }
    fflush(stdout);
}

static void run_input_loop(SDL_Window* window, [[maybe_unused]] SDL_Renderer* renderer) {
    printf("===========================================\n");
    printf("Goggles Manual Input (X11 Backend)\n");
    printf("Tests pointer constraints via XWayland\n");
    printf("===========================================\n");
    printf("1   - Toggle pointer lock (relative mode)\n");
    printf("2   - Toggle mouse grab (confine)\n");
    printf("3   - Print current state\n");
    printf("ESC - Quit\n");
    printf("===========================================\n");
    fflush(stdout);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                printf("[Input] Quit event received\n");
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    printf("[Input] ESC pressed, exiting\n");
                    running = false;
                } else if (event.key.scancode == SDL_SCANCODE_1) {
                    toggle_pointer_lock(window);
                } else if (event.key.scancode == SDL_SCANCODE_2) {
                    toggle_mouse_grab(window);
                } else if (event.key.scancode == SDL_SCANCODE_3) {
                    print_state(window);
                } else {
                    print_event_info(event);
                }
            } else {
                print_event_info(event);
            }
        }

        SDL_Delay(16);
    }
}

auto main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) -> int {
    fprintf(stderr, "[goggles_manual_input_x11] Starting (X11 backend)\n");

    auto vk = init_vulkan();
    fprintf(stderr, "[goggles_manual_input_x11] Vulkan instance created\n");

    auto sdl = init_sdl();
    fprintf(stderr, "[goggles_manual_input_x11] SDL initialized\n");

    const char* display = getenv("DISPLAY");
    const char* wayland_display = getenv("WAYLAND_DISPLAY");
    fprintf(stderr, "[goggles_manual_input_x11] DISPLAY='%s'\n", display ? display : "NULL");
    fprintf(stderr, "[goggles_manual_input_x11] WAYLAND_DISPLAY='%s'\n",
            wayland_display ? wayland_display : "NULL");

    run_input_loop(sdl.window, sdl.renderer);

    fprintf(stderr, "[goggles_manual_input_x11] Exiting\n");
    return 0;
}
