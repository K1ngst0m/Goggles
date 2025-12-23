# Tasks: add-async-capture-worker

## Phase 1: Core Implementation

- [x] 1.1 Add `GOGGLES_CAPTURE_ASYNC` environment variable check
- [x] 1.2 Add `AsyncCaptureItem` struct with timeline semaphore fields
- [x] 1.3 Add timeline semaphore to `SwapData` struct
- [x] 1.4 Add sync fence fallback to `SwapData` struct
- [x] 1.5 Remove per-frame fences and semaphores from `FrameData`
- [x] 1.6 Add `timeline_value` tracking to `FrameData`
- [x] 1.7 Add worker thread state to `CaptureManager`
- [x] 1.8 Implement `worker_func()` with `WaitSemaphoresKHR`
- [x] 1.9 Implement eager worker thread startup in constructor
- [x] 1.10 Create timeline semaphore in `init_export_image()`
- [x] 1.11 Add sync fence fallback in `init_export_image()`
- [x] 1.12 Modify `capture_frame()` async path with timeline semaphore signaling
- [x] 1.13 Modify `capture_frame()` sync fallback path with per-swapchain fence
- [x] 1.14 Update `destroy_frame_resources()` to use timeline semaphore wait

## Phase 2: Shutdown & Cleanup

- [x] 2.1 Implement idempotent `shutdown()` method with compare-and-swap
- [x] 2.2 Implement destructor delegating to `shutdown()`
- [x] 2.3 Add queue drain logic on worker shutdown
- [x] 2.4 Clean up timeline semaphore in `cleanup_swap_data()`
- [x] 2.5 Clean up sync fence in `cleanup_swap_data()`
- [x] 2.6 Add `empty()` method to `SPSCQueue`

## Phase 3: Testing & Validation

- [X] 3.1 Manual testing: basic capture (async mode)
- [X] 3.2 Manual testing: basic capture (sync mode fallback)
- [X] 3.3 Manual testing: rapid swapchain recreation
- [X] 3.4 Manual testing: prolonged capture
- [X] 3.5 Manual testing: shutdown stress

## Phase 4: Documentation

- [x] 4.1 Update vk-layer-capture spec delta
