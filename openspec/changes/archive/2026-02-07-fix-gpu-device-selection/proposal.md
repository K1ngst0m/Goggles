# Change: Fix GPU device selection for multi-GPU systems

## Why

On multi-GPU systems, goggles may select a GPU that cannot properly present to the display surface, causing "device lost" errors. The current logic selects the first device with required extensions, ignoring whether it's actually suitable for the Wayland/X11 surface.

## What Changes

- Add `--gpu` CLI option to explicitly select a GPU by index or name substring
- Improve device selection to prefer GPUs that can actually present to the surface
- Log available GPUs at startup for user visibility

## Impact

- Affected specs: `app-window`
- Affected code: `src/render/backend/vulkan_backend.cpp`, `src/app/cli.cpp`
