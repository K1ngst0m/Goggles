# vk-layer-capture Specification

## Purpose
TBD - created by archiving change add-vk-layer-capture-minimal. Update Purpose after archive.
## Requirements
### Requirement: Vulkan Layer Registration
The capture layer SHALL provide a valid Vulkan layer manifest that allows the Vulkan loader to discover and load the layer.

#### Scenario: Layer discovery by loader
- **GIVEN** `goggles_layer.json` is in `VK_LAYER_PATH`
- **WHEN** a Vulkan application initializes with `VK_INSTANCE_LAYERS=VK_LAYER_goggles_capture`
- **THEN** the Vulkan loader SHALL load `libgoggles_vklayer.so`
- **AND** the layer SHALL negotiate interface version successfully

#### Scenario: Layer negotiation
- **WHEN** the loader calls `vkNegotiateLoaderLayerInterfaceVersion`
- **THEN** the layer SHALL return `VK_SUCCESS`
- **AND** report a supported interface version (>= 2)

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

#### Scenario: Synchronization with present
- **WHEN** the copy command is submitted
- **THEN** the layer SHALL wait on the application's present semaphores
- **AND** signal its own semaphore after copy completes
- **AND** pass that semaphore to the actual present call
- **AND** use a fence to track command buffer completion for reuse

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

