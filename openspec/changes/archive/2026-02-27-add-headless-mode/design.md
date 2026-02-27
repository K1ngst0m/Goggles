## Context

The Goggles viewer currently creates an SDL3 window on every startup path. `VulkanBackend::create()` accepts `SDL_Window*` and builds a `vk::SurfaceKHR` from it, then constructs a swapchain tied to that surface. `ImGuiLayer` also requires the window. There is no path through `Application::create()` that skips these two subsystems.

The `CompositorServer` is already surfaceless — it uses `wlr_headless_backend` and is unaffected by this change. The coupling to SDL lives entirely in `VulkanBackend` (surface + swapchain) and `Application` (SDL init, ImGui init).

## Goals / Non-Goals

**Goals:**
- `goggles --headless --frames N --output path.png -- <app>` runs and exits cleanly
- No SDL window, no ImGui, no swapchain created in headless mode
- Filter chain renders to an offscreen `vk::Image`; final frame is read back and written as PNG
- Existing windowed mode is fully unaffected
- All fallible operations return `tl::expected<T, Error>`

**Non-Goals:**
- Streaming output (video, network sink) — offscreen image is write-once at end of N frames
- Selecting output format other than PNG
- Per-frame PNG export (only the last frame is written)
- Modifying the compositor, filter chain, or shader system internals

## Decisions

### Decision 1: Separate `VulkanBackend::create_headless()` factory

**Chosen:** Add a static `create_headless(RenderSettings) -> ResultPtr<VulkanBackend>` factory alongside the existing `create(SDL_Window*, RenderSettings)`.

**Rationale:** Passing `nullptr` for the window adds implicit, fragile nullable logic throughout. A distinct factory makes the two modes structurally different — the headless instance never holds a `vk::SurfaceKHR` or swapchain members, avoiding dead state. The existing factory signature and call sites are completely unchanged.

**Alternative considered:** `std::optional<SDL_Window*>` parameter — rejected because it leaks the optionality into every call site and all internal methods that gate on the surface.

### Decision 2: Single offscreen image, not double-buffered

**Chosen:** Allocate one `vk::Image` as the render target, with `MAX_FRAMES_IN_FLIGHT = 1` for the headless path (or simply a single command buffer + fence).

**Rationale:** The swapchain double-buffering exists to overlap CPU record and GPU present. Without `vkQueuePresentKHR`, there is nothing to overlap with. A single image + fence-wait-before-record is simpler and correct.

### Decision 3: `vk::Format::eB8G8R8A8Unorm` for the offscreen image

**Chosen:** Fixed format `eB8G8R8A8Unorm` for the offscreen render target and staging buffer.

**Rationale:** `stb_image_write_png` writes RGBA8 directly; `eB8G8R8A8Unorm` maps to BGRA8 on the GPU, requiring a channel swap during readback (or using `eR8G8B8A8Unorm` to avoid it). Using `eR8G8B8A8Unorm` avoids any channel conversion code and maps directly to stb's expected layout.

**Correction:** Use `vk::Format::eR8G8B8A8Unorm` as the offscreen image format to avoid channel-swap logic during PNG readback.

### Decision 4: `readback_to_png` as a method on `VulkanBackend`

**Chosen:** `auto readback_to_png(std::filesystem::path output) -> tl::expected<void, Error>` on `VulkanBackend`.

**Rationale:** The staging buffer allocation and `vkCmdCopyImageToBuffer` require access to the device, queue, command pool, and offscreen image — all owned by `VulkanBackend`. Keeping the readback inside the backend avoids exposing Vulkan internals to `Application`.

### Decision 5: Signal handling via `signalfd` + existing run loop

**Chosen:** In headless mode, open a `signalfd` for `SIGTERM` and `SIGINT` and poll it with zero timeout each tick alongside the compositor frame poll.

**Rationale:** `signalfd` integrates cleanly with the existing poll-based loop without shared state or async-signal-safety concerns. No `std::atomic` flag or global variable needed. The fd is wrapped in `UniqueFd`.

**Alternative considered:** `std::atomic<bool> g_shutdown` set in a `sigaction` handler — simpler but relies on async-signal-safe guarantee of the write; `signalfd` is cleaner in a C++ context.

### Decision 6: Frame count semantics

**Chosen:** `--frames N` counts **compositor frames delivered** (non-zero `get_presented_frame()` returns), not render ticks. The last delivered frame is exported.

**Rationale:** The compositor delivers frames only when the target app commits a surface. Counting render ticks could export a black frame if the app hasn't committed yet. Counting delivered frames guarantees the PNG reflects actual app output.

**Warm-up:** The first compositor frame is accepted immediately; no discard/warm-up count is applied. If the app needs warm-up frames before its output is stable, callers can set `--frames` to a higher value.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Target app takes too long to produce its first frame; `--frames N` hangs | Add `--timeout <seconds>` in a follow-up; for now document that callers should set a process-level timeout |
| Offscreen image layout transition missed → readback returns garbage | Explicit barrier sequence in `readback_to_png`: `eColorAttachmentOptimal → eTransferSrcOptimal` before copy, `eTransferSrcOptimal → eColorAttachmentOptimal` after |
| `stb_image_write_png` write failure not propagated | `stb_image_write_png` returns 0 on failure; check return value and propagate as `tl::expected` error |
| Headless Vulkan device selection skips surface-support check | Physical device selection in headless path checks only DMA-BUF + external memory extensions; no present-queue requirement |
| `signalfd` blocked by `SIG_DFL` reset after `fork()` in child spawn | `signalfd` is opened after `spawn_target_app()`; child inherits nothing from the fd since it is not `O_CLOEXEC`-exempt by default |

## Open Questions

- Should `--frames 0` mean "run until child exits" (no PNG output) as a future capability? Currently `--frames` is required alongside `--output`; both should be required together or both absent.
- Should the offscreen image resolution be configurable separately from `--app-width`/`--app-height`? Currently the output resolution equals the compositor output resolution.
