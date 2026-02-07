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

The layer SHALL intercept `vkCreateDevice` to establish dispatch chains and add required extensions.

#### Scenario: Device creation with extensions

- **WHEN** the target application calls `vkCreateDevice`
- **THEN** the layer SHALL add required device extensions:
  - `VK_KHR_EXTERNAL_MEMORY_FD`
  - `VK_EXT_EXTERNAL_MEMORY_DMA_BUF`
  - `VK_KHR_external_semaphore`
  - `VK_KHR_external_semaphore_fd`
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

#### Scenario: Early Connection Check
- **WHEN** `on_present` is called
- **THEN** the layer SHALL check the socket connection status first
- **AND** if not connected and connection attempt fails, return immediately
- **AND** skip export image initialization and frame capture

### Requirement: Layer Logging Constraints
The layer SHALL follow project logging policies for capture layer code, with explicit opt-in runtime
logging controls suitable for an implicit Vulkan layer.

#### Scenario: Minimal hot-path logging
- **WHEN** `vkQueuePresentKHR` executes
- **THEN** no logging SHALL occur

#### Scenario: Default-off logging
- **GIVEN** `GOGGLES_DEBUG_LOG` is unset or `0`
- **WHEN** any `LAYER_*` logging macro is invoked
- **THEN** no output SHALL be emitted

#### Scenario: Enable logging with default level
- **GIVEN** `GOGGLES_DEBUG_LOG=1` is set
- **AND** `GOGGLES_DEBUG_LOG_LEVEL` is unset
- **WHEN** a log macro at `info` level or higher is invoked
- **THEN** the layer SHALL emit the log message
- **AND** prefix all logs with `[goggles_vklayer]`

#### Scenario: Level filtering
- **GIVEN** `GOGGLES_DEBUG_LOG=1` is set
- **AND** `GOGGLES_DEBUG_LOG_LEVEL=error` is set
- **WHEN** a `debug`, `info`, or `warn` log macro is invoked
- **THEN** no output SHALL be emitted
- **AND** `error` and `critical` logs MAY be emitted

#### Scenario: Launcher forwards options to target app
- **GIVEN** Goggles is launched in default mode with `--layer-log`
- **WHEN** the target app is spawned
- **THEN** Goggles SHALL set `GOGGLES_DEBUG_LOG=1` in the target app environment
- **AND** the capture layer MAY emit `info/warn/debug` logs according to
  `GOGGLES_DEBUG_LOG_LEVEL`

#### Scenario: Efficient backend
- **GIVEN** logging is enabled
- **WHEN** the layer emits a log message
- **THEN** the layer SHOULD use a `write(2)`-based backend
- **AND** SHOULD avoid `stdio` to reduce lock contention

#### Scenario: Anti-spam logging
- **GIVEN** logging is enabled
- **WHEN** an error condition occurs repeatedly in a loop
- **THEN** the layer SHOULD support anti-spam logging primitives (e.g., log once / every N)

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

### Requirement: WSI Proxy Mode Configuration

The layer SHALL provide a WSI proxy mode that virtualizes all window system integration calls.

#### Scenario: WSI proxy mode activation

- **GIVEN** `GOGGLES_WSI_PROXY=1` environment variable is set
- **AND** `GOGGLES_CAPTURE=1` is also set
- **WHEN** the layer initializes
- **THEN** WSI proxy mode SHALL be enabled
- **AND** all surface creation calls SHALL be intercepted

#### Scenario: WSI proxy mode disabled by default

- **GIVEN** `GOGGLES_WSI_PROXY` environment variable is not set
- **WHEN** the layer initializes
- **THEN** WSI proxy mode SHALL be disabled
- **AND** surface creation calls SHALL pass through to the driver

### Requirement: Virtual Surface Resolution Configuration

The layer SHALL allow configuring the virtual surface resolution via environment variables and runtime requests.

#### Scenario: Default resolution

- **GIVEN** WSI proxy mode is enabled
- **AND** `GOGGLES_WIDTH` and `GOGGLES_HEIGHT` are not set
- **AND** no runtime resolution request has been received
- **WHEN** a virtual surface is created
- **THEN** the surface SHALL have resolution 1920x1080

#### Scenario: Custom resolution via environment

- **GIVEN** WSI proxy mode is enabled
- **AND** `GOGGLES_WIDTH` is set to a positive integer
- **AND** `GOGGLES_HEIGHT` is set to a positive integer
- **WHEN** a virtual surface is created
- **THEN** the surface SHALL have the specified resolution

#### Scenario: Runtime resolution override

- **GIVEN** WSI proxy mode is enabled
- **AND** a virtual surface exists
- **WHEN** a valid resolution request is received from the viewer
- **THEN** the surface resolution SHALL be updated to the requested values
- **AND** this SHALL override any environment variable settings

### Requirement: Virtual Surface Creation

The layer SHALL intercept platform-specific surface creation calls and return virtual surfaces when WSI proxy mode is enabled.

#### Scenario: X11 surface virtualization

- **GIVEN** WSI proxy mode is enabled
- **WHEN** the application calls `vkCreateXlibSurfaceKHR`
- **THEN** the layer SHALL NOT create a real X11 surface
- **AND** SHALL return a synthetic VkSurfaceKHR handle
- **AND** no window SHALL be displayed

#### Scenario: Wayland surface virtualization

- **GIVEN** WSI proxy mode is enabled
- **WHEN** the application calls `vkCreateWaylandSurfaceKHR`
- **THEN** the layer SHALL NOT create a real Wayland surface
- **AND** SHALL return a synthetic VkSurfaceKHR handle

#### Scenario: XCB surface virtualization

- **GIVEN** WSI proxy mode is enabled
- **WHEN** the application calls `vkCreateXcbSurfaceKHR`
- **THEN** the layer SHALL NOT create a real XCB surface
- **AND** SHALL return a synthetic VkSurfaceKHR handle

### Requirement: Virtual Surface Capability Queries

The layer SHALL return valid capability information for virtual surfaces.

#### Scenario: Surface capabilities query

- **GIVEN** a virtual surface exists
- **WHEN** the application calls `vkGetPhysicalDeviceSurfaceCapabilitiesKHR`
- **THEN** the layer SHALL return capabilities with:
  - `minImageCount` of 2
  - `maxImageCount` of 3
  - `currentExtent` matching configured resolution
  - `supportedUsageFlags` including `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT` and `VK_IMAGE_USAGE_TRANSFER_SRC_BIT`

#### Scenario: Surface formats query

- **GIVEN** a virtual surface exists
- **WHEN** the application calls `vkGetPhysicalDeviceSurfaceFormatsKHR`
- **THEN** the layer SHALL return at least `VK_FORMAT_B8G8R8A8_SRGB` and `VK_FORMAT_B8G8R8A8_UNORM`

#### Scenario: Present modes query

- **GIVEN** a virtual surface exists
- **WHEN** the application calls `vkGetPhysicalDeviceSurfacePresentModesKHR`
- **THEN** the layer SHALL return `VK_PRESENT_MODE_FIFO_KHR` and `VK_PRESENT_MODE_IMMEDIATE_KHR`

#### Scenario: Surface support query

- **GIVEN** a virtual surface exists
- **WHEN** the application calls `vkGetPhysicalDeviceSurfaceSupportKHR`
- **THEN** the layer SHALL return `VK_TRUE` for all queue families with graphics capability

### Requirement: Virtual Swapchain Creation

The layer SHALL create DMA-BUF exportable images for virtual swapchains.

#### Scenario: Virtual swapchain creation
- **GIVEN** a virtual surface exists
- **WHEN** the application calls `vkCreateSwapchainKHR` with that surface
- **THEN** the layer SHALL create VkImages with `VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT`
- **AND** it SHOULD prefer `VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT` when supported for the requested `VkFormat`
- **AND** it SHALL export DMA-BUF file descriptors for each image
- **AND** it SHALL retrieve and store per-image `stride` and `offset` via `vkGetImageSubresourceLayout`
- **AND** if DRM modifier tiling is used, it SHALL retrieve and store the selected DRM format modifier via `vkGetImageDrmFormatModifierPropertiesEXT`
- **AND** it SHALL return a synthetic VkSwapchainKHR handle

### Requirement: Virtual Swapchain Presentation

The layer SHALL send DMA-BUF frames to the Goggles application on present.

#### Scenario: Virtual present
- **GIVEN** a virtual swapchain exists
- **WHEN** the application calls `vkQueuePresentKHR`
- **THEN** the layer SHALL send the presented image's DMA-BUF fd to Goggles
- **AND** it SHALL include `stride`, `offset`, and `modifier` metadata matching the exported image layout
- **AND** it SHALL NOT present to any physical display
- **AND** it SHALL return `VK_SUCCESS`

### Requirement: Virtual Swapchain Frame Rate Limiting

The layer SHALL provide frame rate limiting for virtual swapchains to prevent runaway frame rates.
When viewer back-pressure pacing is available, the frame rate limiter SHALL act as an upper bound.

#### Scenario: Default frame rate limit

- **GIVEN** WSI proxy mode is enabled
- **AND** `GOGGLES_FPS_LIMIT` environment variable is not set
- **WHEN** the application calls `vkAcquireNextImageKHR`
- **THEN** the layer SHALL limit acquisition rate to 60 FPS

#### Scenario: Custom frame rate limit

- **GIVEN** WSI proxy mode is enabled
- **AND** `GOGGLES_FPS_LIMIT` is set to a positive integer
- **WHEN** the application calls `vkAcquireNextImageKHR`
- **THEN** the layer SHALL limit acquisition rate to the specified FPS

#### Scenario: Disable frame rate limit

- **GIVEN** WSI proxy mode is enabled
- **AND** `GOGGLES_FPS_LIMIT=0` is set
- **WHEN** the application calls `vkAcquireNextImageKHR`
- **THEN** the layer SHALL NOT limit acquisition rate

### Requirement: Present Frame Dump Directory

The dump output directory SHALL be configurable via `GOGGLES_DUMP_DIR`.

#### Scenario: Default dump directory

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is set to a non-empty value
- **AND** `GOGGLES_DUMP_DIR` is not set or is an empty string
- **WHEN** a selected frame is dumped
- **THEN** the layer SHALL write dumps under `/tmp/goggles_dump`

#### Scenario: Custom dump directory

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is set to a non-empty value
- **AND** `GOGGLES_DUMP_DIR` is set to a non-empty value
- **WHEN** a selected frame is dumped
- **THEN** the layer SHALL write dumps under that directory

### Requirement: Present Frame Dump Configuration

The capture layer SHALL support dumping the presented image to disk when
`GOGGLES_DUMP_FRAME_RANGE` is set to a non-empty value.

#### Scenario: Dumping disabled by default

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is not set or is an empty string
- **WHEN** `vkQueuePresentKHR` is called
- **THEN** the layer SHALL NOT dump any images to disk

#### Scenario: Dumping enabled when range is set

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is set to a non-empty value
- **WHEN** a frame is presented whose frame number matches the range
- **THEN** the layer SHALL dump that presented image to disk

### Requirement: Present Frame Dump Outputs

When dumping a selected frame, the layer SHALL write both:
- a `ppm` image file
- a metadata sidecar file with `.ppm.desc` suffix

Both files SHALL share the same base name `{processname}_{frameid}` and be written to the same
directory.

For dumping, `{frameid}` SHALL be the frame number used for capture metadata
(`CaptureFrameMetadata.frame_number`) for that frame.

#### Scenario: Image and metadata outputs are produced

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE=3`
- **AND** the process name is `vkcube`
- **WHEN** frame number 3 is dumped
- **THEN** the layer SHALL write `vkcube_3.ppm`
- **AND** it SHALL write `vkcube_3.ppm.desc`

### Requirement: Present Frame Dump Range Syntax

`GOGGLES_DUMP_FRAME_RANGE` SHALL support selecting frames using a union of:
- single frame numbers (e.g. `3`)
- comma-separated lists (e.g. `3,5,8`)
- inclusive ranges (e.g. `8-13`)

Whitespace around commas and hyphens MAY be ignored.

For the purpose of `GOGGLES_DUMP_FRAME_RANGE`, the “frame number” SHALL be the value used by the
layer for capture metadata (`CaptureFrameMetadata.frame_number`) for that frame.

#### Scenario: Single frame selection

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE=3`
- **WHEN** the layer observes frame numbers 1, 2, 3, 4
- **THEN** it SHALL dump frame 3
- **AND** it SHALL NOT dump frames 1, 2, or 4

#### Scenario: Multi-frame list selection

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE=3,5,8`
- **WHEN** the layer observes frame numbers 1 through 9
- **THEN** it SHALL dump frames 3, 5, and 8
- **AND** it SHALL NOT dump other frames

#### Scenario: Inclusive range selection

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE=8-13`
- **WHEN** the layer observes frame numbers 1 through 14
- **THEN** it SHALL dump frames 8 through 13 (inclusive)
- **AND** it SHALL NOT dump frames 7 or 14

### Requirement: Present Frame Dump Metadata Format

The `.ppm.desc` file SHALL be plain text with one `key=value` pair per line to avoid requiring
additional serialization dependencies.

#### Scenario: Metadata includes core fields

- **GIVEN** a frame is dumped
- **WHEN** the layer writes the `.desc` file
- **THEN** it SHALL include a `frame_number` key
- **AND** it SHALL include a `width` key
- **AND** it SHALL include a `height` key
- **AND** it SHALL include a `format` key
- **AND** it SHALL include a `stride` key
- **AND** it SHALL include an `offset` key
- **AND** it SHALL include a `modifier` key

### Requirement: Present Frame Dump Mode

The dump output format SHALL be controlled by `GOGGLES_DUMP_FRAME_MODE`.

#### Scenario: Default dump mode

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is set
- **AND** `GOGGLES_DUMP_FRAME_MODE` is not set or is an empty string
- **WHEN** a selected frame is dumped
- **THEN** the layer SHALL write a `ppm` dump

#### Scenario: Unsupported dump mode fallback

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is set
- **AND** `GOGGLES_DUMP_FRAME_MODE` is set to an unsupported value
- **WHEN** a selected frame is dumped
- **THEN** the layer SHALL fall back to `ppm`

### Requirement: Present Frame Dump Compatibility

Present frame dumping SHALL work in both WSI proxy and non-WSI-proxy capture paths.

#### Scenario: Dumping in WSI proxy mode

- **GIVEN** WSI proxy mode is enabled (`GOGGLES_WSI_PROXY=1` and `GOGGLES_CAPTURE=1`)
- **AND** `GOGGLES_DUMP_FRAME_RANGE` selects a frame number
- **WHEN** the application presents a virtual swapchain image
- **THEN** the layer SHALL dump the presented image to disk

#### Scenario: Dumping in non-WSI-proxy mode

- **GIVEN** WSI proxy mode is disabled
- **AND** `GOGGLES_DUMP_FRAME_RANGE` selects a frame number
- **WHEN** the application presents a real swapchain image
- **THEN** the layer SHALL dump the presented image to disk

### Requirement: Present Frame Dump Performance Constraints

Dumping SHALL be implemented such that `vkQueuePresentKHR` does not perform file I/O or logging.

#### Scenario: No file I/O or logging on present hook

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is set to a non-empty value
- **WHEN** `vkQueuePresentKHR` is called
- **THEN** the layer SHALL NOT perform file I/O on the present hook thread
- **AND** it SHALL NOT log on the present hook thread
- **AND** any disk writes SHALL be performed asynchronously (off-thread)

### Requirement: Virtual Swapchain Sync Pacing

When WSI proxy mode is enabled, the layer SHALL pace `vkAcquireNextImageKHR` using viewer-provided
back-pressure based on cross-process synchronization primitives.

#### Scenario: Back-pressure pacing

- **GIVEN** WSI proxy mode is enabled
- **AND** the viewer has provided valid synchronization primitives to the layer
- **WHEN** the application calls `vkAcquireNextImageKHR`
- **THEN** the layer SHALL block acquisition until the viewer indicates it has consumed the
  previous frame

#### Scenario: Pacing fallback

- **GIVEN** WSI proxy mode is enabled
- **AND** synchronization primitives are unavailable or the viewer is unresponsive
- **WHEN** the application calls `vkAcquireNextImageKHR`
- **THEN** the layer SHALL fall back to its local CPU-based limiter

### Requirement: Dynamic Resolution Request Protocol

The capture protocol SHALL support resolution request messages from the viewer to the layer.

#### Scenario: Protocol backward compatibility

- **GIVEN** an older layer that does not support resolution requests
- **WHEN** the viewer sends a `CaptureControl` with `resolution_request = 1`
- **THEN** the older layer SHALL ignore the unknown fields
- **AND** continue normal capture operation

#### Scenario: Resolution request message format

- **GIVEN** the viewer wants to request a new resolution
- **WHEN** the viewer sends a `CaptureControl` message
- **THEN** the message SHALL contain:
  - `resolution_request` flag set to 1
  - `requested_width` with desired width in pixels
  - `requested_height` with desired height in pixels

### Requirement: WSI Proxy Dynamic Resolution

The layer SHALL support changing virtual surface resolution at runtime when in WSI proxy mode.

#### Scenario: Resolution change request handling

- **GIVEN** WSI proxy mode is enabled
- **AND** a virtual surface exists
- **WHEN** the layer receives a `CaptureControl` with `resolution_request = 1`
- **THEN** the layer SHALL update the virtual surface's configured resolution
- **AND** subsequent `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` calls SHALL return the new resolution

#### Scenario: Swapchain invalidation on resolution change

- **GIVEN** WSI proxy mode is enabled
- **AND** a virtual swapchain exists
- **WHEN** the virtual surface resolution changes
- **THEN** the next `vkAcquireNextImageKHR` SHALL return `VK_ERROR_OUT_OF_DATE_KHR`
- **AND** the application SHALL recreate the swapchain

#### Scenario: Resolution change ignored in non-proxy mode

- **GIVEN** WSI proxy mode is disabled (normal capture mode)
- **WHEN** the layer receives a `CaptureControl` with `resolution_request = 1`
- **THEN** the layer SHALL ignore the resolution request
- **AND** continue capturing at the application's native resolution

### Requirement: Cross-Process Semaphore Export

The layer SHALL create and export timeline semaphores for cross-process GPU synchronization with the Goggles application.

#### Scenario: Exportable semaphore creation

- **WHEN** capture is initialized for a swapchain
- **THEN** the layer SHALL create two timeline semaphores with `VK_SEMAPHORE_TYPE_TIMELINE`
- **AND** use `VkExportSemaphoreCreateInfo` with `VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT`
- **AND** name them `frame_ready` (Layer signals) and `frame_consumed` (Goggles signals)

#### Scenario: Semaphore FD export

- **WHEN** exportable semaphores are created
- **THEN** the layer SHALL export file descriptors via `vkGetSemaphoreFdKHR`
- **AND** store the FDs for transfer to Goggles

#### Scenario: Required extensions

- **WHEN** `vkCreateDevice` is hooked
- **THEN** the layer SHALL add `VK_KHR_external_semaphore` to enabled extensions
- **AND** add `VK_KHR_external_semaphore_fd` to enabled extensions

### Requirement: Semaphore IPC Transfer

The layer SHALL transfer semaphore file descriptors to the Goggles application via Unix socket.

#### Scenario: Semaphore init message

- **WHEN** the first frame is captured after connection
- **THEN** the layer SHALL send a `semaphore_init` message via IPC
- **AND** attach two FDs via `SCM_RIGHTS`: `[frame_ready_fd, frame_consumed_fd]`
- **AND** include the initial timeline value (0)

#### Scenario: One-time transfer

- **GIVEN** semaphore FDs have been sent for this connection
- **WHEN** subsequent frames are captured
- **THEN** the layer SHALL NOT resend semaphore FDs
- **AND** SHALL send only frame metadata with timeline values

### Requirement: Bidirectional GPU Synchronization

The layer SHALL use the exported semaphores for bidirectional synchronization with Goggles.

#### Scenario: Back-pressure wait

- **GIVEN** frame N is being captured
- **AND** N > 1
- **WHEN** `vkQueuePresentKHR` is called
- **THEN** the layer SHALL wait on `frame_consumed` semaphore for value N-1
- **AND** use a timeout of 100ms to detect disconnection
- **AND** skip the frame if timeout occurs

#### Scenario: Frame ready signal

- **WHEN** the copy command is submitted
- **THEN** the layer SHALL signal `frame_ready` semaphore with value N
- **AND** increment the frame counter

#### Scenario: Frame metadata transfer

- **WHEN** the copy command is submitted
- **THEN** the layer SHALL send `frame_metadata` message via IPC
- **AND** include width, height, format, stride, offset, modifier
- **AND** include the frame number (timeline value N)

### Requirement: Semaphore Reconnection Handling

The layer SHALL handle client reconnection by resetting semaphore state.

#### Scenario: Disconnect detection

- **WHEN** `vkWaitSemaphoresKHR` times out
- **OR** IPC send fails
- **THEN** the layer SHALL mark semaphores as not sent
- **AND** reset frame counter to 0

#### Scenario: Re-export on reconnection

- **WHEN** a new client connects after disconnect
- **THEN** the layer SHALL call `vkGetSemaphoreFdKHR` again for new FDs
- **AND** send `semaphore_init` message to the new client
- **AND** resume sync from frame 1

