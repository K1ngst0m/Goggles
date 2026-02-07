## 1. Investigation

- [x] 1.1 Identify which viewer sync primitives can be reused in WSI proxy mode
- [x] 1.2 Define WSI proxy pacing fallback behavior when viewer is unresponsive

## 2. Specification

- [x] 2.1 Update `vk-layer-capture` delta spec with WSI proxy sync pacing requirements

## 3. Implementation

- [x] 3.1 Add or reuse a device-level timeline semaphore pair for WSI proxy pacing
- [x] 3.2 Block `WsiVirtualizer::acquire_next_image()` on viewer back-pressure (frame_consumed)
- [x] 3.3 Keep `GOGGLES_FPS_LIMIT` as a secondary cap
- [x] 3.4 Preserve existing `sleep_until` path as fallback

## 4. Documentation

- [x] 4.1 Update `docs/dmabuf_sharing.md` to describe WSI proxy sync pacing and fallback

## 5. Validation

- [x] 5.1 Run `openspec validate update-wsi-proxy-sync-pacing --strict`
- [x] 5.2 Run unit tests (`pixi run test -p test`) if impacted tests exist
