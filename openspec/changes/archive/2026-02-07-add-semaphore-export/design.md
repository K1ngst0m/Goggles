# Design: Cross-Process Timeline Semaphore Export

## Context

The capture layer needs efficient GPU synchronization with the Goggles application. Current architecture uses CPU-side semaphore wait in worker thread before IPC, adding latency.

**Constraints:**
- Layer is pure C API (no vulkan-hpp)
- Must use OPAQUE_FD handle type (same driver requirement)
- Handle reconnection gracefully

## Goals / Non-Goals

**Goals:**
- Export two timeline semaphores for bidirectional GPU sync
- Implement back-pressure to throttle Layer when Goggles is slow
- Send semaphore FDs once per connection (not per frame)

**Non-Goals:**
- Support SYNC_FD handle type (less portable)

**Deprecation:**
- Async worker thread (replaced by cross-process semaphore sync)
- Sync fence fallback mode

## Decisions

### Two Semaphores vs Single Semaphore

**Decision:** Use two separate timeline semaphores.

**Rationale:**
- `frame_ready`: Layer signals N, Goggles waits N
- `frame_consumed`: Goggles signals N, Layer waits N-1
- Clear separation of concerns, easier debugging
- Single semaphore with dual values is more complex

### Sync Flow

```
Frame N:
  Layer:   wait(frame_consumed, N-1) → copy → signal(frame_ready, N) → send metadata
  Goggles: recv metadata → wait(frame_ready, N) → render → signal(frame_consumed, N)
```

### Protocol Changes

New message types in `capture_protocol.hpp`:

```cpp
enum class CaptureMessageType : uint32_t {
    client_hello = 1,
    texture_data = 2,
    control = 3,
    semaphore_init = 4,    // NEW: carries two semaphore FDs
    frame_metadata = 5,    // NEW: per-frame data with timeline value
};

struct CaptureSemaphoreInit {
    CaptureMessageType type = CaptureMessageType::semaphore_init;
    uint32_t version = 1;
    uint64_t initial_value = 0;
    // Two FDs via SCM_RIGHTS: [frame_ready_fd, frame_consumed_fd]
};

struct CaptureFrameMetadata {
    CaptureMessageType type = CaptureMessageType::frame_metadata;
    uint32_t width, height;
    VkFormat format;
    uint32_t stride, offset;
    uint64_t modifier;
    uint64_t frame_number;  // Timeline value for this frame
};
```

### Required Extensions

Layer must enable:
- `VK_KHR_external_semaphore`
- `VK_KHR_external_semaphore_fd`

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| OPAQUE_FD requires same driver | Document requirement, fail gracefully |
| Deadlock on disconnect | Use timeout in WaitSemaphoresKHR (100ms) |

## Open Questions

- Timeout value for back-pressure wait (currently 100ms)