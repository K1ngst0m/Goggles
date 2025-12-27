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
    VulkanInstance(VulkanInstance&&) = default;
    VulkanInstance& operator=(VulkanInstance&&) = default;
};

struct SDLContext {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    SDLContext() = default;

    ~SDLContext() {
        if (renderer) {
            SDL_DestroyRenderer(renderer);
        }
        if (window) {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }

    SDLContext(const SDLContext&) = delete;
    SDLContext& operator=(const SDLContext&) = delete;
    SDLContext(SDLContext&&) = default;
    SDLContext& operator=(SDLContext&&) = default;
};

} // namespace

static auto init_vulkan() -> VulkanInstance {
    VulkanInstance vk;

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Goggles Input Test";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    VkResult result = vkCreateInstance(&create_info, nullptr, &vk.handle);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "[goggles_input_test] vkCreateInstance failed: %d\n", result);
        std::exit(1);
    }

    return vk;
}

static auto init_sdl() -> SDLContext {
    SDLContext sdl;

    setenv("SDL_VIDEODRIVER", "x11", 1);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "[goggles_input_test] SDL_Init failed: %s\n", SDL_GetError());
        std::exit(1);
    }

    sdl.window = SDL_CreateWindow("Goggles Input Test", 1280, 720, SDL_WINDOW_RESIZABLE);
    if (!sdl.window) {
        fprintf(stderr, "[goggles_input_test] SDL_CreateWindow failed: %s\n", SDL_GetError());
        std::exit(1);
    }

    sdl.renderer = SDL_CreateRenderer(sdl.window, nullptr);
    if (!sdl.renderer) {
        fprintf(stderr, "[goggles_input_test] SDL_CreateRenderer failed: %s\n",
                SDL_GetError());
        std::exit(1);
    }

    SDL_SetRenderDrawColor(sdl.renderer, 0, 0, 0, 255);
    SDL_RenderClear(sdl.renderer);
    SDL_RenderPresent(sdl.renderer);

    return sdl;
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

static void run_input_loop([[maybe_unused]] SDL_Window* window,
                           [[maybe_unused]] SDL_Renderer* renderer) {
    printf("===========================================\n");
    printf("Goggles Input Test\n");
    printf("Press keys or move mouse to test input forwarding\n");
    printf("Press ESC to quit\n");
    printf("===========================================\n");
    fflush(stdout);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                printf("[Input] Quit event received\n");
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN &&
                       event.key.scancode == SDL_SCANCODE_ESCAPE) {
                printf("[Input] ESC pressed, exiting\n");
                running = false;
            } else {
                print_event_info(event);
            }
        }

        SDL_Delay(16);
    }
}

auto main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) -> int {
    fprintf(stderr, "[goggles_input_test] Starting\n");

    // Initialize Vulkan FIRST to trigger layer loading
    auto vk = init_vulkan();
    fprintf(stderr, "[goggles_input_test] Vulkan instance created (layer loaded)\n");

    // NOW init SDL - layer has already set DISPLAY=:1
    auto sdl = init_sdl();
    fprintf(stderr, "[goggles_input_test] SDL initialized\n");

    const char* display = getenv("DISPLAY");
    fprintf(stderr, "[goggles_input_test] DISPLAY='%s'\n", display ? display : "NULL");

    run_input_loop(sdl.window, sdl.renderer);

    fprintf(stderr, "[goggles_input_test] Exiting\n");
    return 0;
}
