## ADDED Requirements

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

## MODIFIED Requirements

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