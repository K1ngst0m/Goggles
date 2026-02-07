## ADDED Requirements

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

## MODIFIED Requirements

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
