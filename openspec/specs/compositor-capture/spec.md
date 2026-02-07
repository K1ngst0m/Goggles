# compositor-capture Specification

## Purpose
TBD - created by archiving change add-non-vulkan-surface-present. Update Purpose after archive.
## Requirements
### Requirement: Non-Vulkan Surface Presentation
The system SHALL render a selected non-Vulkan client surface (Wayland or XWayland) into the
viewer using the compositor capture path when compositor presentation is available.

#### Scenario: Present selected surface
- **GIVEN** a non-Vulkan client surface is connected to the compositor
- **AND** the surface is selected via the existing surface selector
- **WHEN** the compositor produces a new frame
- **THEN** the viewer presents the selected surface

#### Scenario: Presentation unavailable
- **GIVEN** compositor presentation cannot be initialized
- **WHEN** non-Vulkan clients connect for input
- **THEN** input forwarding continues without presenting non-Vulkan frames

### Requirement: DMA-BUF Export for Compositor Frames
The compositor capture path SHALL export frames using DMA-BUF for zero-copy presentation.

#### Scenario: Export compositor frame via DMA-BUF
- **WHEN** the compositor renders a frame for the selected surface
- **THEN** it exports a DMA-BUF with width, height, format, stride, and modifier metadata
- **AND** the viewer imports and presents the frame without CPU readback

### Requirement: Vulkan Layer Path Unchanged
The compositor capture path SHALL NOT alter the Vulkan layer capture behavior.

#### Scenario: Vulkan capture unaffected
- **GIVEN** a Vulkan application is captured via the layer
- **WHEN** compositor capture is enabled
- **THEN** the Vulkan capture path continues to function as before

