# Change: Use VK_KHR_present_wait for Frame Pacing

## Why

Goggles currently defaults to mailbox present mode and allows uncapped frame submission. On high-end GPUs this drives extremely high FPS, wasting power and producing inconsistent frame pacing. Vulkan's VK_KHR_present_wait lets us explicitly pace presentation and reduce unnecessary GPU/CPU work.

## What Changes

- Prefer FIFO present mode with VK_KHR_present_wait when supported.
- Use VK_KHR_present_wait to pace presentation to `render.target_fps` (0 = uncapped).
- Fallback behavior when VK_KHR_present_wait is unavailable:
  - Prefer MAILBOX present mode.
  - Apply CPU-side frame cap using `render.target_fps` (0 = uncapped).
  - If MAILBOX is unavailable, use FIFO without present wait.
- Add CLI override for `render.target_fps` (mirrors config; 0 = uncapped).

## Impact

- Affected specs: `render-pipeline`, `app-window`.
- Affected code:
  - `src/render/backend/vulkan_backend.cpp` - present mode selection and pacing integration.
  - `src/render/backend/vulkan_backend.hpp` - present wait capability state.
  - `src/util/config.hpp` + `src/util/config.cpp` - allow `target_fps = 0` for uncapped.
  - `src/app/cli.hpp` - CLI override for target fps.
  - `src/app/main.cpp` - apply CLI override and log.
