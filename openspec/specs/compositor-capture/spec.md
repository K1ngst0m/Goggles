# compositor-capture Specification

## Purpose
This spec defines normative requirements for compositor capture behavior in Goggles.
## Requirements
### Requirement: Non-Vulkan Surface Presentation
The system SHALL render a selected non-Vulkan client surface (Wayland or XWayland) into the
viewer using the compositor capture path when compositor presentation is available.

The render pass for compositor-presented frames SHALL composite surfaces in the following order:
1. Mapped `wlr-layer-shell-unstable-v1` surfaces on the `background` layer
2. Mapped `wlr-layer-shell-unstable-v1` surfaces on the `bottom` layer
3. The primary capture surface tree (xdg_toplevel or XWayland surface)
4. XWayland override-redirect popup surfaces belonging to the primary surface
5. Mapped `wlr-layer-shell-unstable-v1` surfaces on the `top` layer
6. Mapped `wlr-layer-shell-unstable-v1` surfaces on the `overlay` layer
7. The compositor software cursor

#### Scenario: Present selected surface
- **GIVEN** a non-Vulkan client surface is connected to the compositor
- **AND** the surface is selected via the existing surface selector
- **WHEN** the compositor produces a new frame
- **THEN** the viewer presents the selected surface

#### Scenario: Overlay layer surface composited above game
- **GIVEN** a game surface is the primary capture target
- **AND** a layer surface with layer `overlay` is mapped
- **WHEN** the compositor produces a frame
- **THEN** the overlay layer surface appears above the game surface in the presented frame

#### Scenario: Background layer surface composited below game
- **GIVEN** a layer surface with layer `background` is mapped
- **WHEN** the compositor produces a frame
- **THEN** the background layer surface appears below the game surface in the presented frame

#### Scenario: No layer surfaces does not affect existing behavior
- **GIVEN** no layer surfaces are connected
- **WHEN** the compositor produces a frame
- **THEN** the presented frame contains only the primary surface tree and cursor (existing behavior)

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

