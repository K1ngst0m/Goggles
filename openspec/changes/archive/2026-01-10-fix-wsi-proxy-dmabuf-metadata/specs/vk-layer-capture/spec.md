## MODIFIED Requirements

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

