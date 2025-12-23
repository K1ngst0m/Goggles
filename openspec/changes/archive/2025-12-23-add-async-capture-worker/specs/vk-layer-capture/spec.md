# vk-layer-capture Specification Delta

## ADDED Requirements

### Requirement: Async Mode Configuration
The layer SHALL provide runtime control over async vs synchronous capture mode.

#### Scenario: Default async mode
- **GIVEN** `GOGGLES_CAPTURE_ASYNC` environment variable is not set
- **WHEN** the layer initializes
- **THEN** async worker mode SHALL be enabled by default

#### Scenario: Explicit async enable
- **GIVEN** `GOGGLES_CAPTURE_ASYNC=1` is set
- **WHEN** the layer initializes
- **THEN** async worker mode SHALL be enabled

#### Scenario: Synchronous fallback
- **GIVEN** `GOGGLES_CAPTURE_ASYNC=0` is set
- **WHEN** the layer initializes
- **THEN** the layer SHALL use synchronous fence wait
- **AND** SHALL NOT spawn worker thread

### Requirement: Timeline Semaphore Synchronization
The layer SHALL use Vulkan timeline semaphores for async GPU synchronization when available.

#### Scenario: Timeline semaphore creation
- **WHEN** export image is initialized for a swapchain
- **THEN** the layer SHALL create a timeline semaphore with `VK_SEMAPHORE_TYPE_TIMELINE`
- **AND** initialize the timeline value to 0

#### Scenario: Timeline semaphore signaling
- **WHEN** `vkQueuePresentKHR` submits the copy command in async mode
- **THEN** the layer SHALL increment the per-swapchain frame counter
- **AND** signal the timeline semaphore with the new value via `VkTimelineSemaphoreSubmitInfo`

#### Scenario: Timeline semaphore waiting
- **WHEN** the worker thread processes a queued item
- **THEN** the worker SHALL call `vkWaitSemaphoresKHR` with the item's timeline value
- **AND** proceed with IPC only after the semaphore reaches that value

#### Scenario: Timeline semaphore fallback
- **WHEN** timeline semaphore creation fails
- **THEN** the layer SHALL fall back to per-swapchain binary fence
- **AND** use synchronous `vkWaitForFences` in the capture path

### Requirement: Async Frame Processing
The layer SHALL use a dedicated worker thread to perform semaphore waiting and IPC operations off the render thread.

#### Scenario: Worker thread initialization
- **WHEN** `CaptureManager` is constructed with async mode enabled
- **THEN** the layer SHALL spawn a worker thread eagerly
- **AND** initialize a lock-free SPSC queue with capacity of 16

#### Scenario: Frame enqueuing on render thread
- **WHEN** `vkQueuePresentKHR` submits the copy command in async mode
- **THEN** the layer SHALL duplicate the DMA-BUF file descriptor via `dup()`
- **AND** push an item containing {device, timeline_sem, timeline_value, dup_fd, metadata} to the queue
- **AND** return immediately without blocking on the semaphore

#### Scenario: Frame processing on worker thread
- **WHEN** the worker thread receives a queued item
- **THEN** the worker SHALL call `vkWaitSemaphoresKHR` with blocking wait
- **AND** send the texture via IPC when the semaphore signals
- **AND** close the duplicated file descriptor

#### Scenario: Queue overflow handling
- **WHEN** the queue is full and a new frame is submitted
- **THEN** the layer SHALL drop the current frame
- **AND** close the duplicated file descriptor immediately
- **AND** wait synchronously on the timeline semaphore to ensure GPU completion

### Requirement: Worker Thread Lifecycle
The layer SHALL manage worker thread lifecycle to ensure clean startup and shutdown.

#### Scenario: Graceful shutdown
- **WHEN** `CaptureManager::shutdown()` is called
- **THEN** the layer SHALL set a shutdown flag atomically
- **AND** notify the worker thread via condition variable
- **AND** join the worker thread before the method returns

#### Scenario: Idempotent shutdown
- **WHEN** `shutdown()` is called multiple times
- **THEN** only the first call SHALL perform shutdown operations
- **AND** subsequent calls SHALL return immediately

#### Scenario: Queue drain on shutdown
- **WHEN** the worker thread receives shutdown signal
- **THEN** the worker SHALL process all remaining queued items
- **AND** close all file descriptors before exiting

### Requirement: Resource Lifetime Management
The layer SHALL ensure file descriptor and Vulkan handle lifetimes are independent between main and worker threads.

#### Scenario: Independent file descriptor lifetime
- **WHEN** enqueueing a frame for async processing
- **THEN** the layer SHALL use `dup()` to create an independent FD
- **AND** the worker thread SHALL close the dup'd FD after use
- **AND** the original FD lifetime SHALL remain tied to swapchain lifetime

## MODIFIED Requirements

### Requirement: GPU Frame Copy
The layer SHALL intercept `vkQueuePresentKHR` and perform GPU-to-GPU copy to the export image.

#### Scenario: Frame copy command recording
- **WHEN** `vkQueuePresentKHR` is called
- **THEN** the layer SHALL record a command buffer with `vkCmdCopyImage`
- **AND** include image memory barriers for layout transitions
- **AND** copy from `PRESENT_SRC_KHR` layout to export image in `TRANSFER_DST_OPTIMAL`

#### Scenario: Async submission with timeline semaphore
- **WHEN** the copy command is submitted in async mode
- **THEN** the layer SHALL signal the timeline semaphore with the frame's timeline value
- **AND** enqueue the frame metadata to the worker thread
- **AND** return immediately to the application without blocking

#### Scenario: Sync submission with fence
- **WHEN** the copy command is submitted in sync mode
- **THEN** the layer SHALL submit with the per-swapchain sync fence
- **AND** wait on the fence before sending via IPC
- **AND** reset the fence for reuse

### Requirement: Layer Logging Constraints
The layer SHALL follow project logging policies for capture layer code.

#### Scenario: Minimal hot-path logging
- **WHEN** `vkQueuePresentKHR` executes
- **THEN** no logging SHALL occur at info level or below
- **AND** only error/critical conditions MAY be logged

#### Scenario: Initialization logging
- **WHEN** `vkCreateInstance` or `vkCreateDevice` is hooked
- **THEN** the layer MAY log at info level with `[goggles_vklayer]` prefix
