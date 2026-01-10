## 1. Implementation
- [x] 1.1 Prefer DRM modifier tiling for WSI proxy virtual swapchain images and record `modifier`, `stride`, `offset`.
- [x] 1.2 Send `CaptureTextureData` with correct `stride`, `offset`, and `modifier` in WSI proxy present path.
- [x] 1.3 Extend `CaptureFrame` to include DMA-BUF `offset` and plumb into Vulkan import.

## 2. Validation
- [x] 2.1 Reproduce and verify modifier propagation with `pixi run start -p release -- /home/kingstom/workspaces/vksdk/1.4.328.1/x86_64/bin/vkcube` (no `vulkan-tools` dependency) and confirm `Capture texture: ... modifier=0x...` matches `DMA-BUF imported: ... modifier=0x...` (see `wsi_proxy_dmabuf_modifier_verification_report.md`).

## 3. OpenSpec Hygiene
- [x] 3.1 Add spec deltas under `openspec/changes/fix-wsi-proxy-dmabuf-metadata/specs/`.
