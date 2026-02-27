## Why

Goggles requires an SDL window for all operation modes, making automated visual testing and CI rendering impossible without a display. A `--headless` mode removes this constraint, enabling the compositor and filter chain to run without any window, display server output, or ImGui layer — producing a PNG snapshot after a configurable number of frames.

## What Changes

- Add `--headless`, `--frames <N>`, and `--output <path>` CLI flags to `CliOptions`
- Add `VulkanBackend` surfaceless factory path: Vulkan instance + device + queue created without `vk::SurfaceKHR`, swapchain, or present resources; an offscreen `vk::Image` (`eB8G8R8A8Unorm`, usage `eColorAttachment | eTransferSrc`) serves as the render target
- Add PNG readback: staging buffer + `vkCmdCopyImageToBuffer` + `stb_image_write_png` after N frames
- `Application::create()` skips `init_sdl()`, `init_imgui_layer()`, and `init_shader_system()` in headless mode; `CompositorServer` starts unchanged
- Headless render loop: polls `get_presented_frame()`, imports DMA-BUF, runs filter chain into offscreen image, no `vkQueuePresentKHR`; exits when N frames rendered or SIGTERM/SIGINT received
- Signal handler (or self-pipe) sets `m_running = false` for clean shutdown (no window close event available)

## Capabilities

### New Capabilities

- `headless-mode`: Surfaceless, windowless operation of the Goggles pipeline — compositor + filter chain render to an offscreen `vk::Image` and export PNG output after N frames; no SDL, no ImGui, no swapchain

### Modified Capabilities

- `app-window`: Application initialization gains a conditional path that skips SDL and ImGui entirely; the SDL window is no longer mandatory for all startup paths
- `render-pipeline`: `VulkanBackend` gains a surfaceless factory that omits `vk::SurfaceKHR`, swapchain creation, and present-queue requirements; `FilterChain::record()` target becomes an offscreen image view instead of a swapchain image view

## Impact

- **`src/app/cli.hpp` / `cli.cpp`**: new fields `headless`, `frames`, `output_path` in `CliOptions`
- **`src/app/application.hpp` / `application.cpp`**: conditional SDL/ImGui skip, new headless run loop, frame counter, PNG export call
- **`src/app/main.cpp`**: signal handler for SIGTERM/SIGINT in headless path
- **`src/render/backend/vulkan_backend.hpp` / `.cpp`**: new `create_headless()` factory or `nullptr`-window branch; offscreen image allocation and `readback_to_png()` method
- **`stb_image_write.h`**: already vendored in `packages/stb`; no new dependencies
- **Policy**: `readback_to_png` is fallible → returns `tl::expected<void, Error>`; offscreen image and staging buffer use RAII wrappers; no `vk::Unique*` or `vk::raii::*`
