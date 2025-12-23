# vk-layer-capture Specification

## Purpose
TBD - created by archiving change add-vk-layer-capture-minimal. Update Purpose after archive.
## Requirements
### Requirement: Vulkan Layer Registration
The capture layer SHALL provide valid Vulkan layer manifests for both 32-bit (i386) and 64-bit (x86_64) architectures that allow the Vulkan loader to discover and load the appropriate layer library.

#### Scenario: Layer discovery by loader (64-bit)
- **GIVEN** `goggles_layer_x86_64.json` is in implicit layer search path
- **WHEN** a 64-bit Vulkan application initializes with `GOGGLES_CAPTURE=1`
- **THEN** the Vulkan loader SHALL load the 64-bit `libgoggles_vklayer.so`
- **AND** the layer SHALL negotiate interface version successfully

#### Scenario: Layer discovery by loader (32-bit)
- **GIVEN** `goggles_layer_i386.json` is in implicit layer search path
- **WHEN** a 32-bit Vulkan application (including Wine/DXVK) initializes with `GOGGLES_CAPTURE=1`
- **THEN** the Vulkan loader SHALL load the 32-bit `libgoggles_vklayer.so`
- **AND** the layer SHALL negotiate interface version successfully

#### Scenario: Layer negotiation
- **WHEN** the loader calls `vkNegotiateLoaderLayerInterfaceVersion`
- **THEN** the layer SHALL return `VK_SUCCESS`
- **AND** report a supported interface version (>= 2)

#### Scenario: Architecture-specific layer names
- **GIVEN** both manifests use the same enable environment variable `GOGGLES_CAPTURE`
- **THEN** the 64-bit manifest SHALL declare layer name `VK_LAYER_goggles_capture_64`
- **AND** the 32-bit manifest SHALL declare layer name `VK_LAYER_goggles_capture_32`

### Requirement: Instance and Device Hooking
The layer SHALL intercept `vkCreateInstance` and `vkCreateDevice` to establish dispatch chains and add required extensions.

#### Scenario: Instance creation with extensions
- **WHEN** the target application calls `vkCreateInstance`
- **THEN** the layer SHALL add `VK_KHR_EXTERNAL_MEMORY_CAPABILITIES` to enabled extensions
- **AND** chain to the next layer/ICD
- **AND** store instance data for dispatch lookup using loader dispatch table pointer

#### Scenario: Device creation with extensions
- **WHEN** the target application calls `vkCreateDevice`
- **THEN** the layer SHALL add required device extensions (`VK_KHR_EXTERNAL_MEMORY_FD`, `VK_EXT_EXTERNAL_MEMORY_DMA_BUF`)
- **AND** enumerate and store all queues
- **AND** identify the graphics queue for capture operations

### Requirement: Swapchain Capture Setup
The layer SHALL intercept `vkCreateSwapchainKHR` to enable frame capture from swapchain images.

#### Scenario: Swapchain creation with transfer usage
- **WHEN** the target application creates a swapchain
- **THEN** the layer SHALL add `VK_IMAGE_USAGE_TRANSFER_SRC_BIT` to imageUsage flags
- **AND** record the swapchain extent, format, and color space
- **AND** retrieve and store the swapchain image handles

#### Scenario: Fallback on usage flag failure
- **WHEN** swapchain creation fails with added usage flags
- **THEN** the layer SHALL retry with original flags
- **AND** disable capture for that swapchain

### Requirement: DMA-BUF Export Image
The layer SHALL create an exportable VkImage for zero-copy frame sharing via DMA-BUF.

#### Scenario: Export image creation
- **WHEN** capture is initiated for a swapchain
- **THEN** the layer SHALL create a VkImage with `VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT`
- **AND** use LINEAR tiling for compatibility
- **AND** match the swapchain extent and format

#### Scenario: DMA-BUF fd export
- **WHEN** the export image memory is allocated
- **THEN** the layer SHALL export a file descriptor via `vkGetMemoryFdKHR`
- **AND** retrieve stride and offset via `vkGetImageSubresourceLayout`

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

### Requirement: Unix Socket IPC
The layer SHALL communicate with the Goggles application via Unix domain socket to transfer DMA-BUF file descriptors.

#### Scenario: Socket connection
- **WHEN** the layer captures its first frame
- **THEN** the layer SHALL connect to abstract socket `/goggles/vkcapture`
- **AND** send client identification data

#### Scenario: DMA-BUF fd transfer
- **WHEN** the export image is ready
- **THEN** the layer SHALL send texture metadata (width, height, format, stride, offset)
- **AND** send the DMA-BUF file descriptor via `SCM_RIGHTS` ancillary data

#### Scenario: Capture control
- **WHEN** the Goggles app sends control data
- **THEN** the layer SHALL start or stop capture based on the control message

### Requirement: Layer Logging Constraints
The layer SHALL follow project logging policies for capture layer code.

#### Scenario: Minimal hot-path logging
- **WHEN** `vkQueuePresentKHR` executes
- **THEN** no logging SHALL occur at info level or below
- **AND** only error/critical conditions MAY be logged

#### Scenario: Initialization logging
- **WHEN** `vkCreateInstance` or `vkCreateDevice` is hooked
- **THEN** the layer MAY log at info level with `[goggles_vklayer]` prefix

### Requirement: Multi-Architecture Build Support
The build system SHALL support building the capture layer for both 32-bit and 64-bit architectures.

#### Scenario: 64-bit layer build (default)
- **WHEN** CMake is invoked without special flags
- **THEN** the build SHALL produce a 64-bit `libgoggles_vklayer.so`

#### Scenario: 32-bit layer build (cross-compile)
- **WHEN** CMake is invoked with `-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-i686.cmake`
- **THEN** the build SHALL produce a 32-bit `libgoggles_vklayer.so`
- **AND** the layer SHALL compile without pointer truncation or format specifier warnings

#### Scenario: Standalone layer build
- **WHEN** the layer is built using the layer-only CMake target
- **THEN** the build SHALL NOT require SDL3, slang, or other main application dependencies
- **AND** only Vulkan headers SHALL be required

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

