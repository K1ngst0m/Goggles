# Change: Update Capture Layer Logic

## Why
The capture layer currently has several efficiency and robustness issues:
1.  **Ignoring Capture State**: Frames are captured and processed even when the viewer is not connected or has requested to stop capturing, wasting GPU resources.
2.  **Inefficient Control Message Polling**: Only a single control message is processed per frame, potentially causing the layer to lag behind the viewer's state.
3.  **Late Connection Checks**: Resource initialization occurs before verifying if a viewer is even connected.
4.  **Implicit Cleanup**: Vulkan resources are only cleaned up when swapchains are destroyed, which may not happen if the layer is unloaded early.

## What Changes
- **Synchronized Capture State**: Added checks for `is_capturing()` in `on_present` to skip all capture logic when inactive.
- **Efficient Control Polling**: Wrapped `recv` in a loop within `poll_control` to process all pending IPC messages.
- **Optimized Connection Check**: Moved connection logic to the beginning of `on_present`.
- **Explicit Shutdown Cleanup**: Added a loop in `CaptureManager::shutdown()` to explicitly free all swapchain-associated Vulkan resources.

## Impact
- Affected specs: `vk-layer-capture`
- Affected code: `src/capture/vk_layer/vk_capture.cpp`, `src/capture/vk_layer/ipc_socket.cpp`
