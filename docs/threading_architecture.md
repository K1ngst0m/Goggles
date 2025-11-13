# Threading Architecture Design

> **Status:** Foundational design established (2025-11-13)
> **Implementation:** Deferred to Prototype 4+ (triggered by profiling data)

## Executive Summary

This document defines Goggles' threading and concurrency model. The design prioritizes **latency predictability over absolute throughput**, using a phased approach that starts simple and scales only when profiling justifies added complexity.

**Core Decisions:**
- **Job System:** BS::thread_pool (Phase 1) → Taskflow (Phase 2+)
- **Inter-Thread Communication:** rigtorp::SPSCQueue (wait-free, real-time)
- **No Coroutines:** Consistent job/queue model for predictable latency
- **Vulkan Threading:** Per-thread command pools + secondary buffers

---

## 1. Design Principles

### 1.1 Latency First

Real-time game capture requires gaming-grade latency (<16.6ms end-to-end). This drives three constraints:

1. **Wait-free primitives:** SPSC queues guarantee bounded operation time
2. **Zero allocations:** No dynamic memory in per-frame code paths
3. **Predictable scheduling:** Explicit job systems over implicit coroutine schedulers

### 1.2 Phased Complexity

Introduce threading infrastructure only when profiling data justifies it:

- **Phase 1:** Offload blocking tasks (encode, I/O) to worker threads
- **Phase 2:** Parallelize Vulkan command buffer generation
- **Phase 3:** (Deferred) Advanced patterns as needed

### 1.3 Single Responsibility

Each subsystem has clear threading responsibilities:

- **Capture layer:** Runs in host app threads (no control)
- **Main thread:** Vulkan submission, presentation, coordination
- **Worker threads:** Task execution from job queue

---

## 2. Library Selection Rationale

### 2.1 Phase 1: BS::thread_pool

**Choice:** [bshoshany/thread-pool](https://github.com/bshoshany/thread-pool) v3.5.0

**Rationale:**
- **Minimal complexity:** 487 lines of code, header-only
- **Sufficient for Phase 1:** Simple job submission without dependencies
- **Low overhead:** ~1µs per job dispatch
- **Easy integration:** CPM-managed, zero external dependencies
- **Low risk:** Easy to understand and debug

**Trade-offs Accepted:**
- No built-in dependency tracking (not needed in Phase 1)
- No work-stealing (acceptable for initial workloads)

### 2.2 Phase 2: Taskflow

**Choice:** [taskflow/taskflow](https://github.com/taskflow/taskflow) v3.10.0

**Rationale:**
- **Active maintenance:** May 2025 release, continuous development
- **Real-time performance:** 61ns task overhead (peer-reviewed)
- **Dependency support:** Task graph with `task_a.precede(task_b)` API
- **Work-stealing:** Handles imbalanced loads (Vulkan command gen pattern)
- **Modern C++:** C++17+ design, fully compatible with project's C++20

**Why NOT enkiTS:**
- Last release: 3+ years ago (vs Taskflow's active releases)
- C++11 constraint (doesn't leverage modern optimizations)
- No published performance benchmarks

**Why NOT oneTBB:**
- **20% real-time penalty** vs hand-coded pools (documented by Intel community)
- Designed for HPC workloads (100,000+ cycle tasks), not fine-grained game tasks
- 10-20× higher overhead than Taskflow

**Upgrade Trigger:**
- Vulkan command buffer count > 300 per frame, OR
- Dependency graph width > 8 concurrent tasks, OR
- Manual dependency tracking code exceeds 500 LOC

### 2.3 Inter-Thread Communication: rigtorp::SPSCQueue

**Choice:** [rigtorp/SPSCQueue](https://github.com/rigtorp/SPSCQueue) v1.2.0

**Rationale:**
- **Wait-free guarantee:** All operations complete in bounded time
- **Throughput:** 112M items/s (20× faster than boost::lockfree)
- **Predictable latency:** No spin-locks, no back-off jitter
- **Perfect for pipelines:** Fixed producer-consumer pairs

**Why NOT MPMC queues:**
- Lock-free ≠ wait-free: MPMC queues have variable latency under contention
- Back-off strategies introduce 50+µs jitter (unacceptable for gaming)
- SPSC + dedicated threads provides determinism

**Architecture Pattern:**
- Use multiple SPSC queues for different pipeline stages
- Each queue connects a dedicated producer to a dedicated consumer
- Example: `Capture Thread → [SPSCQueue] → Encode Thread`

---

## 3. Phased Implementation Roadmap

### Phase 1: Foundational Multi-Threading

**Trigger:** Main thread CPU time consistently exceeds 8ms per frame

**Duration:** 2-4 person-days

**Goals:**
- Offload blocking operations (encode, file I/O) to worker threads
- Establish job system infrastructure
- Implement N-buffering for frame data

**Implementation:**
1. Integrate BS::thread_pool via CMake
2. Create `src/util/job_system.hpp` wrapper
3. Implement SPSC queue for frame metadata passing
4. Add N-buffering (2-3 frames in flight)
5. Migrate encode job to worker thread

**Code Pattern:**
```cpp
// Submit encode job
void submit_encode_job(FrameHandle frame) {
    job_pool.enqueue([frame]() {
        encode_frame(frame);
        release_buffer(frame);
    });
}

// Frame metadata queue
rigtorp::SPSCQueue<FrameMetadata*> capture_to_encode_q(16);

// Producer (capture thread)
FrameMetadata* frame = acquire_buffer();
// ... fill frame data ...
capture_to_encode_q.push(frame);

// Consumer (encode job)
if (auto frame = capture_to_encode_q.try_pop()) {
    encode(*frame);
    release_buffer(frame);
}
```

**Success Metrics:**
- Main thread CPU time < 8ms per frame
- Job dispatch overhead < 1µs
- SPSC queue operations < 100ns

**Riskiest Component:** N-buffering logic and frame lifecycle management

---

### Phase 2: Parallel Vulkan Command Generation

**Trigger:** Profiling shows `RecordCommands` as bottleneck (>3ms CPU time)

**Duration:** 5-10 person-days

**Goals:**
- Parallelize Vulkan command buffer recording across worker threads
- Maintain single-threaded queue submission (Vulkan requirement)
- Upgrade to Taskflow if dependency management becomes complex

**Implementation:**
1. Refactor rendering into parallelizable jobs (per pass, per region)
2. Create per-thread VkCommandPool in worker threads
3. Use secondary command buffers for parallel recording
4. Main thread synchronizes and submits all buffers

**Vulkan Threading Pattern:**
```cpp
// Per-thread command pool (thread-local storage)
thread_local VkCommandPool command_pool = nullptr;

void render_job(TileRegion region, VkCommandBuffer secondary) {
    if (!command_pool) {
        command_pool = create_command_pool(device);
    }

    vkBeginCommandBuffer(secondary, ...);
    // ... record region-specific commands ...
    vkEndCommandBuffer(secondary);
}

// Main thread orchestration
void render_frame() {
    // Submit jobs for parallel recording
    for (auto& region : regions) {
        job_pool.enqueue([&]() {
            render_job(region, secondary_buffers[i]);
        });
    }

    // Synchronize (wait for all jobs)
    job_pool.wait();

    // Main thread submits to queue (Vulkan requirement)
    vkCmdExecuteCommands(primary, secondary_buffers.size(), secondary_buffers.data());
    vkQueueSubmit(queue, ...);
}
```

**Vulkan Constraints:**
- `vkQueueSubmit` is **not thread-safe** (must serialize)
- Each thread needs its own VkCommandPool
- Secondary command buffers allow parallel recording

**Success Metrics:**
- Command recording time reduced by (N-1)/N where N = worker threads
- Queue submission remains single-threaded
- No Vulkan validation errors

**Riskiest Component:** Task dependency tracking without race conditions

---

### Phase 3: Reserved for Future Needs

Potential future patterns (not currently planned):

- Async compute for independent shader passes
- Multi-GPU synchronization
- Advanced pipelining (N+2 frames in flight)

These will be evaluated only if profiling shows clear bottlenecks that justify added complexity.

---

## 4. Performance Budgets

### 4.1 Latency Targets

| Component | Budget | Measurement |
|-----------|--------|-------------|
| **End-to-end** | <16.6ms | Capture → Present for 60fps |
| **Main thread per-frame** | <8ms | CPU time (excluding GPU sync) |
| **Job dispatch** | <1µs | Enqueue operation |
| **SPSC push/pop** | <100ns | Single operation |
| **IPC metadata** | <100µs | Cross-thread transfer |

### 4.2 Real-Time Constraints

**Per-frame code paths (main thread render loop) MUST NOT:**
- Perform dynamic memory allocations (new/malloc/std::make_shared)
- Use blocking synchronization (std::mutex, std::condition_variable)
- Call into kernel (syscalls, file I/O)

**Worker thread code paths SHOULD:**
- Pre-allocate resources during initialization
- Use lock-free/wait-free primitives for coordination
- Bound worst-case execution time

### 4.3 Measurement Strategy

Use **Tracy Profiler** for instrumentation:

```cpp
// Frame boundaries
FrameMark;

// Zone timing
ZoneScoped;

// Custom metrics
TracyPlot("QueueDepth", queue.size());
```

**Key Metrics:**
- Per-frame timing breakdown (capture, process, submit, present)
- CPU core utilization per worker thread
- Queue depth and time-in-queue histograms
- Lock contention (should be zero in real-time paths)

---

## 5. Threading Model Policy

### 5.1 Job System Mandate

**All concurrent processing MUST use the project's central job system.**

Direct use of `std::thread` or `std::jthread` for pipeline work is **prohibited**.

**Exception:** External integration code (networking, IPC) outside the real-time path may use `std::jthread` with RAII guarantees.

### 5.2 Main Thread Responsibilities

The main thread owns:
- Vulkan instance, device, swapchain lifecycle
- Queue submission (`vkQueueSubmit`)
- Window events and user input
- Job coordination (submit, wait, synchronize)

**The main thread MUST NOT:**
- Block on I/O operations
- Perform heavy computation (>1ms)
- Allocate memory in per-frame code

### 5.3 Worker Thread Responsibilities

Worker threads execute jobs from the queue:
- Encode captured frames
- Record Vulkan secondary command buffers
- Process shader pipeline passes (future)

**Worker threads MUST NOT:**
- Submit to Vulkan queues (main thread only)
- Modify shared state without synchronization
- Use blocking primitives in real-time paths

### 5.4 Capture Layer Threading

The Vulkan layer runs in the **host application's threads** (no control):

- Hook functions (`vkQueuePresentKHR`) execute on app threads
- Layer state MUST be thread-safe (atomics, locks as needed)
- Minimize time in hooks (capture metadata, signal main app, return immediately)

---

## 6. No Coroutines Decision

### 6.1 Rationale

C++20 coroutines are **explicitly rejected** for Goggles:

**Performance Issues:**
- 3-4× slower than equivalent iterator-based code
- Hidden heap allocations on suspension (unacceptable for real-time)
- Frame allocation overhead is unpredictable

**Suitability:**
- Excellent for async I/O (network, disk)
- Poor for latency-sensitive graphics pipelines

**Project Decision:**
- Job system + SPSC queues provide **consistent, predictable latency**
- Coroutines introduce allocation overhead we cannot tolerate

### 6.2 Alternative Considered

Original plan included coroutines for IPC (Prototype 4+). Research showed:

- Async I/O benefits don't outweigh allocation cost
- Job system + SPSC queues handle IPC with lower complexity
- Consistent threading model is easier to reason about

**Verdict:** Job/queue model everywhere. No coroutines.

---

## 7. Vulkan-Specific Threading Patterns

### 7.1 Command Pool Per Thread

Vulkan command pools are **not thread-safe**. Each worker thread gets its own pool:

```cpp
namespace goggles::render {

class CommandPoolManager {
public:
    VkCommandPool get_for_current_thread() {
        thread_local VkCommandPool pool = nullptr;
        if (!pool) {
            pool = create_command_pool(m_device);
        }
        return pool;
    }

private:
    VkDevice m_device;
};

}
```

### 7.2 Secondary Command Buffers

Parallelize command recording with secondary buffers:

```cpp
// Main thread: create primary buffer
VkCommandBuffer primary = ...;
vkBeginCommandBuffer(primary, ...);

// Submit jobs to record secondary buffers
std::vector<VkCommandBuffer> secondaries(num_jobs);
for (size_t i = 0; i < num_jobs; ++i) {
    job_system.submit([=]() {
        VkCommandBuffer secondary = allocate_secondary_buffer();
        record_commands_for_region(secondary, regions[i]);
        secondaries[i] = secondary;
    });
}

// Wait for all jobs
job_system.wait();

// Execute secondaries in primary
vkCmdExecuteCommands(primary, secondaries.size(), secondaries.data());
vkEndCommandBuffer(primary);

// Submit (only main thread)
vkQueueSubmit(queue, ...);
```

### 7.3 Queue Submission Serialization

**Vulkan queues are NOT thread-safe.** Only the main thread may call `vkQueueSubmit`.

Pattern:
1. Workers record commands in parallel
2. Workers signal completion (atomic counter, semaphore)
3. Main thread waits for all workers
4. Main thread submits all buffers in single call

---

## 8. Dependency Management

### 8.1 CMake Integration

Dependencies are managed via CPM.cmake:

```cmake
# Phase 1: Simple job submission
CPMAddPackage(
    NAME BS_thread_pool
    GITHUB_REPOSITORY bshoshany/thread-pool
    VERSION 3.5.0
    OPTIONS
        "BUILD_TESTS OFF"
        "BUILD_EXAMPLES OFF"
)

# Phase 1: Wait-free queues
CPMAddPackage(
    NAME rigtorp_SPSCQueue
    GITHUB_REPOSITORY rigtorp/SPSCQueue
    VERSION 1.2.0
)

# Phase 2: Dependency-aware scheduling
CPMAddPackage(
    NAME taskflow
    GITHUB_REPOSITORY taskflow/taskflow
    VERSION 3.10.0
    OPTIONS
        "TF_BUILD_EXAMPLES OFF"
        "TF_BUILD_TESTS OFF"
)
```

### 8.2 Upgrade Path

- **Phase 1 → Phase 2:** BS::thread_pool → Taskflow
  - Change: Wrapper API in `src/util/job_system.hpp`
  - Impact: Callsites remain unchanged (abstraction preserved)
  - Risk: Low (Taskflow API is similar)

---

## 9. Risk Assessment

### 9.1 Phase 1 Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| N-buffering bugs | Medium | High | Extensive testing, ASAN builds |
| Queue overflow | Low | Medium | Sized based on worst-case latency |
| Job system overhead | Low | Low | Profiling validates <1µs dispatch |

### 9.2 Phase 2 Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Race conditions | Medium | High | ThreadSanitizer, careful review |
| Dependency tracking errors | Medium | Medium | Start with simple linear dependencies |
| Vulkan validation errors | Low | High | Per-thread pools, secondary buffers |

---

## 10. Decision Log

### 2025-11-13: Foundation Established

**Decisions Made:**
1. Job system approach: BS::thread_pool → Taskflow (phased)
2. IPC: rigtorp::SPSCQueue for all real-time paths
3. No coroutines: consistent job/queue model
4. Taskflow over enkiTS: active maintenance + performance data
5. Reject oneTBB: 20% real-time penalty documented

**Rationale:**
- Latency predictability > absolute throughput
- Phased approach defers complexity until justified
- Wait-free primitives guarantee bounded operation time

**Next Steps:**
- Implement skeleton infrastructure (`job_system.hpp`, `queues.hpp`)
- Update PROJECT_POLICIES.md with threading constraints
- Wait for profiling trigger before Phase 1 implementation

---

## References

- [Taskflow: A Lightweight Parallel and Heterogeneous Task Programming System](https://tsung-wei-huang.github.io/papers/tpds21-taskflow.pdf) (IEEE TPDS 2021)
- [rigtorp::SPSCQueue Performance Analysis](https://rigtorp.se/ringbuffer/)
- [Vulkan Threading Guidelines](https://www.khronos.org/registry/vulkan/specs/1.3/html/chap3.html#fundamentals-threadingbehavior)
- [Game Engine Architecture Patterns](https://www.ea.com/frostbite/news/framegraph-extensible-rendering-architecture-in-frostbite) (Frostbite)
- [OBS Studio Capture Architecture](https://github.com/obsproject/obs-studio)

---

**Document Status:** Living document, will be updated as implementation progresses.
