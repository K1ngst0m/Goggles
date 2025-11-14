# Threading Usage Guide

> **Status:** Fully implemented and tested
> **Libraries:** BS::thread_pool v3.5.0, rigtorp::SPSCQueue v1.1

## Overview

This document provides practical examples for using Goggles' threading infrastructure. The threading system is fully implemented with real BS::thread_pool and rigtorp::SPSCQueue integrations, ready for use in capture and pipeline modules.

---

## Quick Start

The threading system is already enabled and ready to use:

```cpp
#include "src/util/job_system.hpp"
#include "src/util/queues.hpp"

// Initialize threading (usually done at app startup)
goggles::util::JobSystem::initialize(4);

// Submit jobs to worker threads  
auto future = goggles::util::JobSystem::submit([]() {
    // Your work here
});

// Create lock-free queues for inter-thread communication
goggles::util::SPSCQueue<FrameData*> queue(16);

// Cleanup (usually done at app exit)
goggles::util::JobSystem::shutdown();
```

## Testing

The threading system includes comprehensive tests that verify functionality:

```bash
# Run all threading tests
cd build/debug && ./tests/goggles_tests "[job_system]"
cd build/debug && ./tests/goggles_tests "[queues]"

# Run full test suite (includes threading + other modules)  
cd build/debug && ./tests/goggles_tests
```

**Test Coverage:**
- **JobSystem:** Initialization, job submission, wait operations, exception handling, performance
- **SPSCQueue:** Basic operations, move semantics, FIFO ordering, multi-threading, performance

---

## JobSystem Usage Examples

### Basic Job Submission

```cpp
#include "src/util/job_system.hpp"

void example_basic_jobs() {
    // Initialize job system (usually done once at app startup)
    goggles::util::JobSystem::initialize(4); // 4 worker threads

    // Submit a simple job
    auto future = goggles::util::JobSystem::submit([]() {
        // Heavy computation here
        encode_frame_data();
    });

    // Continue main thread work...
    update_ui();

    // Wait for job completion if needed
    future.wait();

    // Shutdown (usually done at app exit)
    goggles::util::JobSystem::shutdown();
}
```

### Fire-and-Forget Jobs

```cpp
void example_fire_and_forget() {
    // Submit without storing future (fire-and-forget)
    goggles::util::JobSystem::submit([captured_data = std::move(frame_data)]() {
        save_to_disk(captured_data);
    });
    
    // Main thread continues immediately
    render_next_frame();
}
```

### Batch Job Processing

```cpp
void example_batch_processing() {
    std::vector<std::future<void>> jobs;
    
    // Submit multiple jobs
    for (const auto& region : tile_regions) {
        auto future = goggles::util::JobSystem::submit([region]() {
            process_tile_region(region);
        });
        jobs.push_back(std::move(future));
    }
    
    // Wait for all jobs to complete
    for (auto& job : jobs) {
        job.wait();
    }
    
    // Alternatively, use wait_all() for convenience
    goggles::util::JobSystem::wait_all();
}
```

---

## SPSCQueue Usage Examples

### Frame Metadata Pipeline

```cpp
#include "src/util/queues.hpp"

struct FrameMetadata {
    uint64_t frame_id;
    void* texture_handle;
    uint32_t width, height;
};

void example_frame_pipeline() {
    // Create queue for frame metadata (power of 2 capacity)
    goggles::util::SPSCQueue<FrameMetadata*> frame_queue(16);
    
    // Producer thread (e.g., capture layer)
    FrameMetadata* frame = acquire_frame_buffer();
    frame->frame_id = current_frame_id++;
    frame->texture_handle = captured_texture;
    
    if (!frame_queue.try_push(frame)) {
        // Queue full - handle overflow
        release_frame_buffer(frame);
        handle_queue_overflow();
    }
    
    // Consumer thread (e.g., encode worker)
    if (auto* frame = frame_queue.try_pop()) {
        encode_frame(*frame);
        release_frame_buffer(frame);
    }
}
```

### Pointer-Based Communication

```cpp
void example_pointer_passing() {
    goggles::util::SPSCQueue<std::unique_ptr<ProcessingJob>> job_queue(8);
    
    // Producer: Create and submit job
    auto job = std::make_unique<ProcessingJob>();
    job->setup_data();
    
    if (!job_queue.try_push(std::move(job))) {
        // Handle queue full condition
        drop_frame_gracefully();
    }
    
    // Consumer: Process jobs
    while (auto job_ptr = job_queue.try_pop()) {
        auto job = std::move(*job_ptr);  // Take ownership
        job->execute();
        // job automatically destroyed when scope exits
    }
}
```

### Queue Sizing Strategy

```cpp
void example_queue_sizing() {
    // For 60fps with 33ms maximum acceptable latency:
    // 2 frames buffering minimum, use power of 2
    size_t capacity = 4;
    
    // For 144fps with 16ms latency:
    // 3-4 frames, round to power of 2
    size_t high_fps_capacity = 8;
    
    goggles::util::SPSCQueue<FrameData*> queue(capacity);
    
    // Monitor queue depth in practice
    if (queue.size() > capacity * 0.75) {
        // Queue filling up - may need larger capacity
        log_queue_pressure_warning();
    }
}
```

---

## Integration Patterns

### Capture → Process → Encode Pipeline

```cpp
class FramePipeline {
private:
    goggles::util::SPSCQueue<FrameData*> capture_to_process_{16};
    goggles::util::SPSCQueue<FrameData*> process_to_encode_{16};
    
public:
    void initialize() {
        goggles::util::JobSystem::initialize();
        
        // Start worker threads for pipeline stages
        start_process_worker();
        start_encode_worker();
    }
    
    void submit_captured_frame(FrameData* frame) {
        if (!capture_to_process_.try_push(frame)) {
            drop_frame(frame);
        }
    }
    
private:
    void start_process_worker() {
        goggles::util::JobSystem::submit([this]() {
            while (running_) {
                if (auto* frame = capture_to_process_.try_pop()) {
                    apply_shader_effects(*frame);
                    
                    if (!process_to_encode_.try_push(frame)) {
                        release_frame(frame);
                    }
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }
    
    void start_encode_worker() {
        goggles::util::JobSystem::submit([this]() {
            while (running_) {
                if (auto* frame = process_to_encode_.try_pop()) {
                    encode_to_file(*frame);
                    release_frame(frame);
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }
};
```

---

## Performance Guidelines

### Real-Time Constraints

```cpp
// ✅ GOOD: Pre-allocated buffers
FrameData frame_buffers[MAX_FRAMES_IN_FLIGHT];
size_t current_buffer = 0;

void capture_frame() {
    FrameData* frame = &frame_buffers[current_buffer];
    current_buffer = (current_buffer + 1) % MAX_FRAMES_IN_FLIGHT;
    
    // Use pre-allocated buffer - no allocations in hot path
    capture_into_buffer(frame);
    submit_to_queue(frame);
}

// ❌ BAD: Dynamic allocation in hot path
void capture_frame_bad() {
    auto frame = std::make_unique<FrameData>(); // Allocation!
    capture_into_buffer(frame.get());
    // This violates real-time constraints
}
```

### Queue Operation Performance

```cpp
void performance_example() {
    goggles::util::SPSCQueue<int> queue(1024);
    
    // ✅ GOOD: Non-blocking operations
    if (queue.try_push(42)) {
        // Success
    } else {
        // Queue full - handle gracefully
    }
    
    if (auto* value = queue.try_pop()) {
        // Process *value
    } else {
        // Queue empty - continue
    }
    
    // ❌ BAD: Blocking operations (not available in our API)
    // queue.push(42);  // Would block if full
    // auto value = queue.pop();  // Would block if empty
}
```

---

## Debugging and Monitoring

### Queue Health Monitoring

```cpp
void monitor_queue_health(const goggles::util::SPSCQueue<FrameData*>& queue) {
    size_t current_size = queue.size();
    size_t capacity = queue.capacity();
    
    double fill_ratio = static_cast<double>(current_size) / capacity;
    
    if (fill_ratio > 0.9) {
        GOGGLES_LOG_WARN("Queue nearly full: {}/{} ({}%)", 
                        current_size, capacity, fill_ratio * 100);
    }
    
    // Track in profiler
    TracyPlot("QueueFillRatio", fill_ratio);
}
```

### Job System Monitoring

```cpp
void monitor_job_system() {
    size_t thread_count = goggles::util::JobSystem::thread_count();
    GOGGLES_LOG_DEBUG("Job system running with {} threads", thread_count);
    
    // Measure job dispatch overhead
    auto start = std::chrono::high_resolution_clock::now();
    goggles::util::JobSystem::submit([](){});
    auto end = std::chrono::high_resolution_clock::now();
    
    auto dispatch_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    TracyPlot("JobDispatchTime_us", dispatch_time.count());
}
```

---

## Phase 2: Taskflow Upgrade

When upgrading to Taskflow for dependency-aware scheduling:

```cpp
// Future Phase 2 usage (not yet implemented)
void example_task_dependencies() {
    // This will be available after Phase 2 implementation
    // auto task_a = submit_task([]{ process_lighting(); });
    // auto task_b = submit_task([]{ process_shadows(); });
    // auto task_c = submit_task([]{ composite_final(); });
    // 
    // task_c.depend_on(task_a, task_b);  // task_c waits for A and B
    // 
    // execute_all_tasks();
}
```

For now, use manual synchronization with futures and `wait_all()`.

---

## Troubleshooting

### Common Issues

1. **Headers don't compile:** Ensure `GOGGLES_THREADING_ENABLED` is defined
2. **Link errors:** Verify CPM dependencies are properly configured
3. **Runtime crashes:** Check queue capacity is power of 2
4. **Performance issues:** Profile to verify threading is beneficial

### Compilation Check

```bash
# Verify threading is properly enabled
cmake --build --preset debug 2>&1 | grep -i "threading"

# Should see: "Threading libraries configured. Recompile to activate."
```

---

This document will be updated as the threading system evolves through Phase 1 and Phase 2 implementations.
