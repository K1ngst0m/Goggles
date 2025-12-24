## ADDED Requirements

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

The layer SHALL allow configuring the virtual surface resolution via environment variables.

#### Scenario: Default resolution

- **GIVEN** WSI proxy mode is enabled
- **AND** `GOGGLES_WIDTH` and `GOGGLES_HEIGHT` are not set
- **WHEN** a virtual surface is created
- **THEN** the surface SHALL have resolution 1920x1080

#### Scenario: Custom resolution

- **GIVEN** WSI proxy mode is enabled
- **AND** `GOGGLES_WIDTH` is set to a positive integer
- **AND** `GOGGLES_HEIGHT` is set to a positive integer
- **WHEN** a virtual surface is created
- **THEN** the surface SHALL have the specified resolution

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
- **AND** SHALL export DMA-BUF file descriptors for each image
- **AND** SHALL return a synthetic VkSwapchainKHR handle

#### Scenario: Swapchain image query

- **GIVEN** a virtual swapchain exists
- **WHEN** the application calls `vkGetSwapchainImagesKHR`
- **THEN** the layer SHALL return the DMA-BUF exportable images

#### Scenario: Next image acquisition

- **GIVEN** a virtual swapchain exists
- **WHEN** the application calls `vkAcquireNextImageKHR`
- **THEN** the layer SHALL return the next available image index
- **AND** signal the provided semaphore or fence immediately

### Requirement: Virtual Swapchain Presentation

The layer SHALL send DMA-BUF frames to the Goggles application on present.

#### Scenario: Virtual present

- **GIVEN** a virtual swapchain exists
- **WHEN** the application calls `vkQueuePresentKHR`
- **THEN** the layer SHALL send the presented image's DMA-BUF fd to Goggles
- **AND** SHALL NOT present to any physical display
- **AND** SHALL return `VK_SUCCESS`

### Requirement: Virtual Swapchain Frame Rate Limiting

The layer SHALL provide frame rate limiting for virtual swapchains to prevent runaway frame rates.

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