# Change: Add Async Capture Worker

## Why
The current capture implementation performs synchronous fence waits in the application's render thread during `vkQueuePresentKHR`, which can cause frame pacing variance that is perceptible to users in low-latency game streaming scenarios.

## What Changes
- Add dedicated worker thread using timeline semaphores for async GPU synchronization
- Move fence waiting and IPC operations off the render thread
- Add `GOGGLES_CAPTURE_ASYNC` environment variable for runtime toggle (default: enabled)
- Replace per-frame fences with single timeline semaphore per swapchain
- Add `empty()` method to `SPSCQueue` for idiomatic queue checks
- Add `shutdown()` method to `CaptureManager` for proper cleanup in shared library context

## Impact
- Affected specs: `vk-layer-capture`
- Affected code: `src/capture/vk_layer/vk_capture.cpp`, `src/capture/vk_layer/vk_capture.hpp`, `src/util/queues.hpp`
