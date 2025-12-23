# Design: Async Capture Worker Thread

## Architecture

```
┌──────────────────────┐          ┌─────────────────────────┐
│  Main Thread         │          │   Worker Thread         │
│  (vkQueuePresent)    │          │                         │
├──────────────────────┤          ├─────────────────────────┤
│ 1. Submit GPU copy   │          │ while (!shutdown) {     │
│    with timeline sem │          │   cv.wait(queue)        │
│ 2. dup(dmabuf_fd)    │          │   item = queue.pop()    │
│ 3. queue.push({      │ ──────>  │   WaitSemaphoresKHR()   │
│      device,         │          │   send_texture(fd)      │
│      timeline_sem,   │          │   close(dup_fd)         │
│      timeline_value, │          │ }                       │
│      dup_fd,         │          │                         │
│      metadata        │          │ Drain queue on exit     │
│    })                │          │                         │
│ 4. notify_one()      │          │                         │
│ 5. return            │          │                         │
└──────────────────────┘          └─────────────────────────┘
```

## Key Design Decisions

### 1. Timeline Semaphores (Not Fences)
**Decision:** Use Vulkan timeline semaphores (`VK_SEMAPHORE_TYPE_TIMELINE`) instead of binary fences.

**Rationale:**
- Single semaphore per swapchain instead of per-frame fences
- More efficient GPU synchronization with monotonic counter
- `WaitSemaphoresKHR` allows waiting on specific timeline value
- Cleaner resource management (no fence reset needed)

**Implementation:**
```cpp
// Per-swapchain timeline semaphore
VkSemaphore timeline_sem = VK_NULL_HANDLE;
uint64_t frame_counter = 0;

// Submit with timeline signal
uint64_t timeline_value = ++swap->frame_counter;
VkTimelineSemaphoreSubmitInfo timeline_submit{};
timeline_submit.pSignalSemaphoreValues = &timeline_value;

// Worker waits on specific value
VkSemaphoreWaitInfo wait_info{};
wait_info.pSemaphores = &item.timeline_sem;
wait_info.pValues = &item.timeline_value;
funcs.WaitSemaphoresKHR(device, &wait_info, timeout);
```

### 2. Eager Worker Thread Initialization
**Decision:** Start worker thread in `CaptureManager` constructor, not lazily on first frame.

**Rationale:**
- Simpler code path with predictable initialization
- Avoids thread spawn latency on first captured frame
- Worker thread idles efficiently via condition variable when queue empty

### 3. Runtime Toggle for Fallback
**Decision:** Provide `GOGGLES_CAPTURE_ASYNC` environment variable to control async vs sync mode.

**Rationale:**
- Safety mechanism if threading issues discovered in production
- Allows performance comparison between sync and async modes
- Standard pattern for Vulkan layer configuration

**Implementation:**
```cpp
static bool should_use_async_capture() {
    static const bool use_async = []() {
        const char* env = std::getenv("GOGGLES_CAPTURE_ASYNC");
        return env == nullptr || std::strcmp(env, "0") != 0;
    }();
    return use_async;
}
```

### 4. Lock-Free Queue (SPSCQueue)
**Decision:** Use existing `goggles::util::SPSCQueue` for main→worker communication.

**Rationale:**
- Already implemented and tested in src/util/queues.hpp
- Lock-free for single producer (main thread) / single consumer (worker)
- No mutex contention on hot path (queue.push)

**Enhancement:** Added `empty()` method for idiomatic queue checks.

### 5. dup() File Descriptor
**Decision:** Duplicate the DMA-buf FD for each queued frame.

**Rationale:**
- Original FD (swap->dmabuf_fd) has swapchain lifetime
- Worker thread may process frame after swapchain destroyed
- dup() creates independent FD with separate refcount

### 6. Sync Fence Fallback
**Decision:** Fall back to per-swapchain fence if timeline semaphores unavailable.

**Rationale:**
- Timeline semaphores require Vulkan 1.2 or `VK_KHR_timeline_semaphore`
- Graceful degradation ensures capture works on older drivers
- Sync fallback uses single fence per swapchain (not per-frame)

## Data Structures

### AsyncCaptureItem
```cpp
struct AsyncCaptureItem {
    VkDevice device = VK_NULL_HANDLE;
    VkSemaphore timeline_sem = VK_NULL_HANDLE;
    uint64_t timeline_value = 0;
    int dmabuf_fd = -1;
    CaptureTextureData metadata{};
};
```

### SwapData Additions
```cpp
// Timeline semaphore for async capture
VkSemaphore timeline_sem = VK_NULL_HANDLE;
uint64_t frame_counter = 0;

// Fence for sync mode fallback
VkFence sync_fence = VK_NULL_HANDLE;
```

### FrameData Changes
```cpp
struct FrameData {
    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
    uint64_t timeline_value = 0;  // Tracks which timeline value this frame signaled
    bool cmd_buffer_busy = false;
};
```

### CaptureManager State
```cpp
// Async worker state
util::SPSCQueue<AsyncCaptureItem> async_queue_{16};
std::thread worker_thread_;
std::atomic<bool> shutdown_{false};
std::mutex cv_mutex_;
std::condition_variable cv_;
```

## Thread Lifecycle

### Startup (Eager)
Worker thread spawned in constructor if async mode enabled:
```cpp
CaptureManager::CaptureManager() {
    if (should_use_async_capture()) {
        shutdown_.store(false, std::memory_order_release);
        worker_thread_ = std::thread(&CaptureManager::worker_func, this);
    }
}
```

### Shutdown
Idempotent `shutdown()` method with compare-and-swap:
```cpp
void CaptureManager::shutdown() {
    bool expected = false;
    if (!shutdown_.compare_exchange_strong(expected, true, std::memory_order_release)) {
        return;  // Already shutdown
    }
    cv_.notify_one();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}
```

## Error Handling

### Queue Full
```cpp
if (!async_queue_.try_push(std::move(item))) {
    close(dup_fd);
    // Wait synchronously to ensure GPU completes
    funcs.WaitSemaphoresKHR(device, &wait_info, Time::infinite);
    return;
}
```

### dup() Failure
```cpp
int dup_fd = dup(swap->dmabuf_fd);
if (dup_fd < 0) {
    // Wait synchronously and skip IPC
    funcs.WaitSemaphoresKHR(device, &wait_info, Time::infinite);
    return;
}
```

### Timeline Semaphore Creation Failure
```cpp
if (res != VK_SUCCESS) {
    swap->timeline_sem = VK_NULL_HANDLE;
    // Fall back to sync fence
    funcs.CreateFence(device, &fence_info, nullptr, &swap->sync_fence);
}
```

## Performance Characteristics

- **Main thread latency**: O(1) - queue.push + notify
- **Worker latency**: GPU-bound (WaitSemaphoresKHR duration)
- **Queue contention**: None (lock-free SPSC)
- **CPU overhead**: Minimal (blocking wait, not polling)
- **Memory overhead**: ~2 MB (thread stack) + ~1.3 KB (queue)
