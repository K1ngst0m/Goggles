#include <SDL3/SDL.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vulkan/vulkan.h>

namespace {

constexpr int WINDOW_COUNT = 3;

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

struct MultiWindowContext {
    std::array<SDL_Window*, WINDOW_COUNT> windows = {};
    std::array<SDL_Renderer*, WINDOW_COUNT> renderers = {};
    bool initialized = false;

    MultiWindowContext() = default;

    ~MultiWindowContext() {
        for (int i = WINDOW_COUNT - 1; i >= 0; --i) {
            if (renderers[static_cast<size_t>(i)]) {
                SDL_DestroyRenderer(renderers[static_cast<size_t>(i)]);
            }
            if (windows[static_cast<size_t>(i)]) {
                SDL_DestroyWindow(windows[static_cast<size_t>(i)]);
            }
        }
        if (initialized) {
            SDL_Quit();
        }
    }

    MultiWindowContext(const MultiWindowContext&) = delete;
    MultiWindowContext& operator=(const MultiWindowContext&) = delete;

    MultiWindowContext(MultiWindowContext&& other) noexcept
        : windows(other.windows), renderers(other.renderers), initialized(other.initialized) {
        other.windows = {};
        other.renderers = {};
        other.initialized = false;
    }

    MultiWindowContext& operator=(MultiWindowContext&& other) noexcept {
        if (this != &other) {
            for (int i = WINDOW_COUNT - 1; i >= 0; --i) {
                if (renderers[static_cast<size_t>(i)]) {
                    SDL_DestroyRenderer(renderers[static_cast<size_t>(i)]);
                }
                if (windows[static_cast<size_t>(i)]) {
                    SDL_DestroyWindow(windows[static_cast<size_t>(i)]);
                }
            }
            if (initialized) {
                SDL_Quit();
            }

            windows = other.windows;
            renderers = other.renderers;
            initialized = other.initialized;

            other.windows = {};
            other.renderers = {};
            other.initialized = false;
        }
        return *this;
    }
};

struct WindowConfig {
    const char* title;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr std::array<WindowConfig, WINDOW_COUNT> WINDOW_CONFIGS = {{
    {"Surface Test 1 (Wayland)", 60, 0, 0},
    {"Surface Test 2 (Wayland)", 0, 60, 0},
    {"Surface Test 3 (Wayland)", 0, 0, 60},
}};

} // namespace

static auto init_vulkan() -> VulkanInstance {
    VulkanInstance vk;

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Goggles Surface Selector Test (Wayland)";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    VkResult result = vkCreateInstance(&create_info, nullptr, &vk.handle);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "[goggles_manual_surface_selector_wayland] vkCreateInstance failed: %d\n",
                result);
        std::exit(1);
    }

    return vk;
}

static auto init_sdl_windows() -> MultiWindowContext {
    MultiWindowContext ctx;

    setenv("SDL_VIDEODRIVER", "wayland", 1);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "[goggles_manual_surface_selector_wayland] SDL_Init failed: %s\n",
                SDL_GetError());
        std::exit(1);
    }
    ctx.initialized = true;

    for (size_t i = 0; i < WINDOW_COUNT; ++i) {
        ctx.windows[i] = SDL_CreateWindow(WINDOW_CONFIGS[i].title, 400, 300, SDL_WINDOW_RESIZABLE);
        if (!ctx.windows[i]) {
            fprintf(stderr,
                    "[goggles_manual_surface_selector_wayland] SDL_CreateWindow failed for window "
                    "%zu: %s\n",
                    i + 1, SDL_GetError());
            std::exit(1);
        }

        ctx.renderers[i] = SDL_CreateRenderer(ctx.windows[i], nullptr);
        if (!ctx.renderers[i]) {
            fprintf(stderr,
                    "[goggles_manual_surface_selector_wayland] SDL_CreateRenderer failed for "
                    "window %zu: %s\n",
                    i + 1, SDL_GetError());
            std::exit(1);
        }

        const auto& cfg = WINDOW_CONFIGS[i];
        SDL_SetRenderDrawColor(ctx.renderers[i], cfg.r, cfg.g, cfg.b, 255);
        SDL_RenderClear(ctx.renderers[i]);
        SDL_RenderPresent(ctx.renderers[i]);
    }

    return ctx;
}

static void print_instructions() {
    printf("===========================================\n");
    printf("Goggles Surface Selector Test (Wayland)\n");
    printf("Tests multi-surface input routing\n");
    printf("===========================================\n");
    printf("This test creates 3 Wayland surfaces.\n");
    printf("Use Goggles F4 key to open surface selector.\n");
    printf("===========================================\n");
    printf("ESC - Quit\n");
    printf("===========================================\n");
    fflush(stdout);
}

static auto find_window_index(const MultiWindowContext& ctx, SDL_WindowID window_id) -> int {
    for (size_t i = 0; i < WINDOW_COUNT; ++i) {
        if (SDL_GetWindowID(ctx.windows[i]) == window_id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static void print_event_info(const MultiWindowContext& ctx, const SDL_Event& event) {
    int window_idx = -1;

    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
        window_idx = find_window_index(ctx, event.key.windowID);
        printf("[Input] KEY DOWN: window=%d scancode=%d name='%s'\n", window_idx + 1,
               event.key.scancode, SDL_GetScancodeName(event.key.scancode));
        break;

    case SDL_EVENT_KEY_UP:
        window_idx = find_window_index(ctx, event.key.windowID);
        printf("[Input] KEY UP: window=%d scancode=%d name='%s'\n", window_idx + 1,
               event.key.scancode, SDL_GetScancodeName(event.key.scancode));
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        window_idx = find_window_index(ctx, event.button.windowID);
        printf("[Input] MOUSE DOWN: window=%d button=%d at (%.1f, %.1f)\n", window_idx + 1,
               event.button.button, event.button.x, event.button.y);
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        window_idx = find_window_index(ctx, event.button.windowID);
        printf("[Input] MOUSE UP: window=%d button=%d at (%.1f, %.1f)\n", window_idx + 1,
               event.button.button, event.button.x, event.button.y);
        break;

    case SDL_EVENT_MOUSE_MOTION:
        window_idx = find_window_index(ctx, event.motion.windowID);
        printf("[Input] MOTION: window=%d pos=(%.1f, %.1f) rel=(%.1f, %.1f)\n", window_idx + 1,
               event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        window_idx = find_window_index(ctx, event.wheel.windowID);
        printf("[Input] WHEEL: window=%d scroll=(%.1f, %.1f)\n", window_idx + 1, event.wheel.x,
               event.wheel.y);
        break;
    }
    fflush(stdout);
}

static void run_event_loop(MultiWindowContext& ctx) {
    print_instructions();

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                printf("[Event] Quit\n");
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    printf("[Event] ESC pressed, exiting\n");
                    running = false;
                } else {
                    print_event_info(ctx, event);
                }
            } else {
                print_event_info(ctx, event);
            }
        }

        SDL_Delay(16);
    }
}

auto main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) -> int {
    fprintf(stderr, "[goggles_manual_surface_selector_wayland] Starting\n");

    const char* wayland_display = getenv("WAYLAND_DISPLAY");
    if (!wayland_display || wayland_display[0] == '\0') {
        fprintf(stderr, "[goggles_manual_surface_selector_wayland] WAYLAND_DISPLAY not set.\n");
        fprintf(stderr, "Run this test via: pixi run start debug <this_executable>\n");
        return 1;
    }
    fprintf(stderr, "[goggles_manual_surface_selector_wayland] WAYLAND_DISPLAY='%s'\n",
            wayland_display);

    auto vk = init_vulkan();
    fprintf(stderr, "[goggles_manual_surface_selector_wayland] Vulkan instance created\n");

    auto ctx = init_sdl_windows();
    fprintf(stderr, "[goggles_manual_surface_selector_wayland] SDL windows created\n");

    run_event_loop(ctx);

    fprintf(stderr, "[goggles_manual_surface_selector_wayland] Exiting\n");
    return 0;
}
