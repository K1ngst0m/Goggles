# Threading

## Purpose

Defines Goggles' threading and concurrency model. The design prioritizes **latency predictability over throughput**, using a phased approach that starts simple and scales when profiling justifies complexity.

## Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         Main Thread                             │
│  - Owns Vulkan device, swapchain                                │
│  - Submits to Vulkan queues (NOT thread-safe)                   │
│  - Coordinates jobs, handles window events                      │
└─────────────────────┬───────────────────────────────────────────┘
                      │ submit jobs
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Job System                                 │
│  Phase 1: BS::thread_pool (simple dispatch)                     │
│  Phase 2: Taskflow (dependency-aware, upgrade when needed)      │
└─────────────────────┬───────────────────────────────────────────┘
                      │
        ┌─────────────┴──────────────┐
        ▼                           ▼
┌───────────────┐           ┌───────────────┐
│ Worker Thread │           │ Worker Thread │
│ - Encode      │           │ - Record cmds │
│ - I/O tasks   │           │ - Processing  │
└───────┬───────┘           └───────────────┘
        │
        │ SPSC Queue (wait-free)
        ▼
┌───────────────┐
│ Frame Data    │
│ (pre-alloc'd) │
└───────────────┘
```

## Key Components

### Job System (`src/util/job_system.hpp`)

Central thread pool for all concurrent work. Direct `std::thread` usage is prohibited for pipeline work.

```cpp
// Initialize at startup
JobSystem::initialize(4);  // 4 workers

// Submit work
auto future = JobSystem::submit([]{ encode_frame(); });

// Wait if needed
future.wait();
```

### SPSC Queue (`src/util/queues.hpp`)

Wait-free queue for inter-thread communication. All operations complete in bounded time.

```cpp
SPSCQueue<FrameData*> queue(16);  // capacity must be power of 2

// Producer
queue.try_push(frame);  // non-blocking

// Consumer
if (auto* f = queue.try_pop()) { process(*f); }
```

## How They Connect

**Capture → Process → Encode Pipeline:**
1. Capture thread produces frame metadata
2. SPSC queue transfers to worker (zero-copy, pointer passing)
3. Worker processes and passes to next stage
4. Main thread coordinates, never blocks

**Vulkan Command Recording (Phase 2):**
1. Main thread creates primary command buffer
2. Workers record secondary buffers in parallel (per-thread command pool)
3. Main thread waits, then executes secondaries
4. Only main thread calls `vkQueueSubmit`

## Constraints / Decisions

### Real-Time Constraints

**Per-frame code (main thread) MUST NOT:**
- Allocate memory (`new`, `malloc`)
- Use blocking sync (`mutex`, `condition_variable`)
- Perform I/O or syscalls

**Workers SHOULD:**
- Use pre-allocated buffers
- Use wait-free primitives
- Bound worst-case execution time

### Performance Budgets

| Operation | Budget |
|-----------|--------|
| End-to-end latency | <16.6ms (60fps) |
| Main thread per-frame | <8ms CPU |
| Job dispatch | <1µs |
| SPSC push/pop | <100ns |

### Why These Libraries

| Choice | Why |
|--------|-----|
| BS::thread_pool | Simple, 487 LOC, <1µs dispatch, sufficient for Phase 1 |
| Taskflow (Phase 2) | Active maintenance, 61ns overhead, dependency graphs |
| Custom SPSC | Wait-free guarantee, 112M items/s, predictable latency |
| No coroutines | Hidden allocations, 3-4× slower, unpredictable |

### Phased Approach

- **Phase 1:** Offload blocking tasks (encode, I/O) when main thread >8ms
- **Phase 2:** Parallelize Vulkan command recording when `RecordCommands` >3ms
- **Upgrade trigger:** Command buffers >300/frame OR dependency graph >8 wide

## Usage Examples

See `tests/util/test_job_system.cpp` and `tests/util/test_queues.cpp` for comprehensive examples.

Quick patterns:

```cpp
// Fire-and-forget
JobSystem::submit([data = std::move(frame)]{ save(data); });

// Batch with sync
std::vector<std::future<void>> jobs;
for (auto& region : regions) {
    jobs.push_back(JobSystem::submit([&]{ process(region); }));
}
for (auto& j : jobs) j.wait();

// Pipeline queues
SPSCQueue<Frame*> capture_to_encode(16);
// Producer: capture_to_encode.try_push(frame);
// Consumer: if (auto* f = capture_to_encode.try_pop()) encode(*f);
```

## References

- [docs/project_policies.md](project_policies.md) - Threading policy (Section E)
- [Taskflow paper](https://tsung-wei-huang.github.io/papers/tpds21-taskflow.pdf) - Performance analysis
- [Vulkan threading](https://www.khronos.org/registry/vulkan/specs/1.3/html/chap3.html#fundamentals-threadingbehavior)
