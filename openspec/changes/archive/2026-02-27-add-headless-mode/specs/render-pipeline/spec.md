## ADDED Requirements

### Requirement: Surfaceless VulkanBackend Factory
`VulkanBackend` SHALL provide a `create_headless(RenderSettings) -> ResultPtr<VulkanBackend>` static factory that creates a Vulkan instance, selects a physical device, and creates a logical device and queue without requiring a `vk::SurfaceKHR`. This factory SHALL NOT create a swapchain, present semaphores, or frame-pacing resources.

#### Scenario: Headless factory succeeds without display
- **GIVEN** a GPU supporting DMA-BUF import and external memory extensions is available
- **WHEN** `VulkanBackend::create_headless(settings)` is called
- **THEN** it SHALL return a valid `VulkanBackend` instance
- **AND** the instance SHALL hold no `vk::SurfaceKHR` or swapchain

#### Scenario: Device selection without present support
- **GIVEN** headless mode is active
- **WHEN** a physical device is selected
- **THEN** the device SHALL be required to support `VK_EXT_external_memory_dma_buf`, `VK_EXT_image_drm_format_modifier`, and `VK_KHR_external_semaphore_fd`
- **AND** surface present support SHALL NOT be a selection criterion

### Requirement: Offscreen Render Target Allocation
When operating in headless mode, `VulkanBackend` SHALL allocate a single `vk::Image` with format `eR8G8B8A8Unorm`, tiling `eOptimal`, and usage `eColorAttachment | eTransferSrc` as the render target. This image SHALL be used as the target passed to `FilterChain::record()` in place of a swapchain image view.

#### Scenario: Offscreen image created at initialization
- **GIVEN** `VulkanBackend::create_headless()` completes
- **WHEN** the first render call is made
- **THEN** the offscreen image SHALL exist in device-local memory
- **AND** its format SHALL be `eR8G8B8A8Unorm`
- **AND** its dimensions SHALL match the configured compositor output resolution

#### Scenario: Filter chain writes to offscreen image
- **GIVEN** headless mode is active and a compositor frame has been imported
- **WHEN** `render()` is called
- **THEN** `FilterChain::record()` SHALL receive the offscreen image view as its render target
- **AND** no swapchain image view SHALL be passed

### Requirement: Headless Frame Submission Without Present
In headless mode, frame submission SHALL queue render commands and wait on a fence for GPU completion. `vkQueuePresentKHR` SHALL NOT be called. Frame pacing via `throttle_present` or `vkWaitForPresentKHR` SHALL NOT be applied.

#### Scenario: Fence-based synchronization replaces present
- **GIVEN** headless mode is active
- **WHEN** a frame's render commands are submitted
- **THEN** `vkWaitForFences` SHALL be called to synchronize before the next frame
- **AND** `vkQueuePresentKHR` SHALL NOT be called

### Requirement: Offscreen Image Readback to PNG
`VulkanBackend` SHALL expose `readback_to_png(std::filesystem::path) -> tl::expected<void, Error>` that copies the offscreen image to a host-visible staging buffer and writes a PNG via `stb_image_write_png`. The image SHALL be transitioned to `eTransferSrcOptimal` before copy and back to `eColorAttachmentOptimal` after.

#### Scenario: Successful readback and PNG write
- **GIVEN** headless mode has completed N render frames
- **WHEN** `readback_to_png("/tmp/out.png")` is called
- **THEN** a valid PNG file SHALL be written to `/tmp/out.png`
- **AND** the image dimensions SHALL match the offscreen image extent

#### Scenario: Staging buffer invalidated before CPU read
- **GIVEN** the staging buffer memory type is not `eHostCoherent`
- **WHEN** the GPU-to-buffer copy completes
- **THEN** `vkInvalidateMappedMemoryRanges` SHALL be called before the CPU reads the buffer
