## 1. CLI Flags

- [x] 1.1 Add `headless` (bool), `frames` (uint32_t), and `output_path` (std::filesystem::path) fields to `CliOptions` in `src/app/cli.hpp`
- [x] 1.2 Register `--headless`, `--frames`, and `--output` with CLI11 in `src/app/cli.cpp`; validate that `--frames` and `--output` are both present when `--headless` is set, returning an error otherwise
- [x] 1.3 Verify: `goggles --headless --frames 5 --output /tmp/x.png --help` prints new flags; `goggles --headless --frames 5 -- app` exits with non-zero and descriptive message about missing `--output`

## 2. VulkanBackend Surfaceless Factory

- [x] 2.1 Add `static auto create_headless(RenderSettings) -> tl::expected<std::unique_ptr<VulkanBackend>, Error>` declaration to `src/render/backend/vulkan_backend.hpp`; mark existing `create(SDL_Window*, RenderSettings)` unchanged
- [x] 2.2 Implement `create_headless()` in `vulkan_backend.cpp`: call `create_instance()` without surface extensions, skip `create_surface()`, call `select_physical_device()` with a headless-aware predicate (no present-queue requirement), then `create_device()`, `create_command_resources()`, `create_sync_objects()`, `init_filter_chain()`
- [x] 2.3 Add `m_headless` bool and offscreen image members (`m_offscreen_image`, `m_offscreen_memory`, `m_offscreen_view`, `m_offscreen_extent`) to `VulkanBackend`; allocate offscreen `vk::Image` (`eR8G8B8A8Unorm`, `eColorAttachment | eTransferSrc`, device-local) in `create_headless()`
- [x] 2.4 Guard `create_swapchain()`, present semaphore creation, and `throttle_present()` behind `!m_headless` checks so they are never called in headless path

## 3. Headless Render Submission

- [x] 3.1 In `VulkanBackend::render()`, add headless branch: skip `acquire_next_image()`; pass `m_offscreen_view` and `m_offscreen_extent` to `FilterChain::record()` instead of swapchain image view; submit commands with a fence; call `vkWaitForFences` before returning — no `vkQueuePresentKHR`
- [x] 3.2 Verify the fence is properly reset with `vkResetFences` before each submission in the headless path

## 4. PNG Readback

- [x] 4.1 Add `auto readback_to_png(std::filesystem::path output) -> tl::expected<void, Error>` declaration to `vulkan_backend.hpp`
- [x] 4.2 Implement `readback_to_png()`: allocate host-visible staging buffer, record commands to transition offscreen image `eColorAttachmentOptimal → eTransferSrcOptimal`, `vkCmdCopyImageToBuffer`, transition back; submit + fence wait; if memory is not host-coherent call `vkInvalidateMappedMemoryRanges`; map buffer, call `stbi_write_png`; check return value and propagate failure as `tl::expected` error; unmap + destroy staging buffer via RAII
- [x] 4.3 Verify: running the full pipeline with a known test client produces a non-empty PNG at the specified output path

## 5. Application Headless Path

- [x] 5.1 In `Application::create()` (`src/app/application.cpp`), add early branch: when `cli_opts.headless == true`, skip `init_sdl()`, `init_imgui_layer()`, and `init_shader_system()`; call `VulkanBackend::create_headless()` instead of `create(window, settings)`
- [x] 5.2 Add `run_headless(uint32_t frames, std::filesystem::path output) -> tl::expected<void, Error>` to `Application`: loop calling `get_presented_frame()`, skipping ticks with no new frame; on new frame call `render_frame()` (headless render path); count delivered frames; after N frames call `m_vulkan_backend->readback_to_png(output)` and return
- [x] 5.3 In `src/app/main.cpp`, after spawning the child app, check `cli_opts.headless`: if true call `app->run_headless(frames, output_path)`, log result, and exit; otherwise run the existing windowed event loop unchanged

## 6. Signal Handling

- [x] 6.1 In the headless path in `main.cpp`, open a `signalfd` for `SIGTERM` and `SIGINT` (block both signals first via `sigprocmask`); wrap the fd in `goggles::util::UniqueFd`
- [x] 6.2 In `run_headless()`, poll the signalfd with zero timeout each tick; on signal receipt, log the signal, terminate the child, and return an `Error` causing main to exit with non-zero code
- [x] 6.3 Verify: `kill -TERM <pid>` while goggles runs in headless mode causes clean shutdown with child process terminated

## 7. Tests

- [x] 7.1 Add CLI parsing tests in `tests/app/test_cli.cpp`: `--headless --frames 10 --output /tmp/x.png` parses correctly; `--headless --frames 10` (missing `--output`) returns error
- [x] 7.2 Add a CTest integration test (disabled in CI via `DEFINED ENV{CI}` guard) that runs `goggles --headless --frames 3 --output /tmp/goggles_headless_test.png -- <solid_color_client>` and verifies exit code 0 and file exists

## 8. Verification

- [x] 8.1 `pixi run build -p test` completes without errors or clang-tidy warnings
- [x] 8.2 `pixi run test -p test` passes all existing unit tests with no regressions
- [x] 8.3 Manual smoke test: `pixi run build -p debug && build/debug/bin/goggles --headless --frames 5 --output /tmp/smoke.png -- vkcube` exits 0 and produces a valid PNG
