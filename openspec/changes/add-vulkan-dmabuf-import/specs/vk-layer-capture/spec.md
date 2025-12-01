## MODIFIED Requirements

### Requirement: Goggles App Frame Display (Temporary)
The Goggles application SHALL receive DMA-BUF frames and display them via Vulkan with zero-copy import. The imported DMA-BUF is rendered directly to the SDL window's Vulkan swapchain.

#### Scenario: DMA-BUF Vulkan import and display
- **GIVEN** the app receives a DMA-BUF fd via socket
- **WHEN** the fd and metadata (width, height, format, stride) are valid
- **THEN** the app SHALL create a VkImage with `VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT`
- **AND** import the fd via `VkImportMemoryFdInfoKHR`
- **AND** blit the imported image to the swapchain
- **AND** present the swapchain image

#### Scenario: Texture metadata change
- **GIVEN** the app has an imported DMA-BUF image
- **WHEN** a new frame arrives with different dimensions or format
- **THEN** the app SHALL destroy the existing imported image
- **AND** create a new VkImage matching the new metadata
- **AND** import the new DMA-BUF fd

#### Scenario: No capture source
- **GIVEN** the Goggles app is running with Vulkan rendering
- **WHEN** no layer has connected to the socket
- **THEN** the app SHALL present a cleared swapchain image
- **AND** continue accepting socket connections

#### Scenario: Layer disconnect
- **GIVEN** a capture layer was connected
- **WHEN** the layer disconnects
- **THEN** the app SHALL destroy the imported image resources
- **AND** present cleared frames until reconnection
