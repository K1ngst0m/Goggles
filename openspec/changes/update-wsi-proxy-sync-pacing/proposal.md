# Change: Update WSI Proxy Pacing to Use Viewer Back-Pressure

## Why

WSI proxy mode (`GOGGLES_WSI_PROXY=1`) currently paces the target application by sleeping in
`WsiVirtualizer::acquire_next_image()` based on a local timestamp and `GOGGLES_FPS_LIMIT`. This is
CPU scheduling-dependent and does not naturally react to viewer load.

The default (non-WSI-proxy) path already uses cross-process timeline semaphores (`frame_ready` and
`frame_consumed`) to provide deterministic back-pressure from the viewer. Reusing the same
mechanism for WSI proxy mode should improve frame pacing stability and reduce jitter.

## What Changes

- Add WSI proxy acquire pacing based on viewer back-pressure using existing cross-process timeline
  semaphores (`frame_consumed` wait).
- Keep `GOGGLES_FPS_LIMIT` as an upper bound; it no longer needs to be the sole pacing mechanism.
- If sync semaphores cannot be established or the viewer becomes unresponsive, fall back to the
  existing `sleep_until` limiter.

## Impact

- Affected specs: `vk-layer-capture`
- Affected code:
  - `src/capture/vk_layer/wsi_virtual.cpp` (acquire pacing)
  - `src/capture/vk_layer/vk_capture.cpp` (shared sync primitives reuse/refactor)
  - `docs/dmabuf_sharing.md` (WSI proxy pacing description)
