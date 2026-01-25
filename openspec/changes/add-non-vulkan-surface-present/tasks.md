## 1. Compositor Presentation Path

- [x] 1.1 Create a headless output swapchain for compositor presentation.
- [x] 1.2 Render the selected surface into the swapchain and export DMA-BUF attributes.
- [x] 1.3 Cache the latest compositor frame for cross-thread retrieval.
- [x] 1.4 Reset cached compositor frames when the selection changes.

## 2. Viewer Integration

- [x] 2.1 Expose the compositor-presented frame via `InputForwarder`.
- [x] 2.2 Convert DRM formats to Vulkan formats in the viewer render path.
- [x] 2.3 Reuse the existing Vulkan backend UI/render path for compositor frames.

## 3. Manual Verification

- [x] 3.1 Build: `pixi run build -p debug`
- [x] 3.2 Launch Goggles with input forwarding enabled.
- [x] 3.5 Use the surface selector to pick the client and confirm it renders + input works.
