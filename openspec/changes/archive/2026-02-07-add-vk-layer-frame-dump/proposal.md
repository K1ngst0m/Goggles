# Change: Add vk_layer Present Frame Dumping

## Why

Capture correctness issues (format, swizzle, alpha, sync) are hard to diagnose without attaching
the full Goggles viewer pipeline. A targeted “dump selected presented frames to disk” option helps
with regression testing and bug reports, and it must work in both WSI proxy and non-WSI-proxy
paths.

## What Changes

- Add `GOGGLES_DUMP_DIR` to select the dump output directory.
  - Default: `/tmp/goggles_dump`.
- Add `GOGGLES_DUMP_FRAME_MODE` to select dump output format.
  - Supported: `ppm` only (for now).
  - Default: `ppm`.
- Add `GOGGLES_DUMP_FRAME_RANGE` to enable dumping and select which frames to dump:
  - Unset / empty → dumping disabled.
  - Supports single numbers (`3`), lists (`3,5,8`), and inclusive ranges (`8-13`).
- In default mode (launching a target app), add CLI flags to configure the launched process:
  - `--dump-dir <dir>` → `GOGGLES_DUMP_DIR`
  - `--dump-frame-range <range>` → `GOGGLES_DUMP_FRAME_RANGE`
  - `--dump-frame-mode <mode>` → `GOGGLES_DUMP_FRAME_MODE`
- When dumping a selected frame, write two artifacts with the same base name:
  - `{processname}_{frameid}.ppm` (image dump)
  - `{processname}_{frameid}.ppm.desc` (image metadata as raw `key=value` pairs)
- Support dumping for:
  - Standard swapchain capture (non-WSI-proxy).
  - WSI proxy mode (`GOGGLES_WSI_PROXY=1`) virtual swapchains.
- Preserve capture layer constraints (per `docs/project_policies.md`):
  - No file I/O and no logging in `vkQueuePresentKHR`.
  - Any disk I/O occurs off-thread.

## Impact

- Affected specs: `vk-layer-capture`, `app-window`
- Affected code:
  - `src/capture/vk_layer/vk_capture.cpp` / `src/capture/vk_layer/vk_capture.hpp`
  - `src/capture/vk_layer/vk_hooks.cpp`
  - `src/capture/vk_layer/wsi_virtual.cpp` / `src/capture/vk_layer/wsi_virtual.hpp`
  - `src/app/cli.hpp`
  - `src/app/main.cpp`
