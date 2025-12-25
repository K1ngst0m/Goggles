# Optimize Capture Present Check

## Summary
Reorder the logic in `CaptureManager::on_present` to check and attempt the IPC socket connection *before* performing expensive resource initialization.

## Motivation
Currently, `CaptureManager::on_present` checks if the export image is initialized and initializes it if necessary (creating images, allocating memory, exporting file descriptors) *before* checking if the capture layer is actually connected to the viewer application.

If the Goggles viewer is not running, the layer still incurs the overhead of:
1.  Querying DRM modifiers.
2.  Creating a `VkImage`.
3.  Allocating `VkDeviceMemory`.
4.  Exporting a DMA-BUF file descriptor.
5.  Creating semaphores and fences.
6.  Creating command pools and buffers.

This logic is wasteful for games running with the layer enabled but without an active viewer. By moving the connection check to the start of `on_present`, we can skip all this work when it's not needed.

## Technical Design
The `CaptureManager::on_present` method in `src/capture/vk_layer/vk_capture.cpp` will be modified to:
1.  Perform the socket connection check (and attempt to connect) immediately after acquiring the lock and finding the swapchain data.
2.  If the socket is not connected (and cannot connect), return early.
3.  Only proceed to `init_export_image` and `capture_frame` if the socket is connected.

## Drawbacks
*   None. This is a pure optimization.

## Alternatives
*   None. The current behavior is demonstrably inefficient.
