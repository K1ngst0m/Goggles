# compositor-capture Specification

## Purpose
Defines how Goggles captures compositor-managed client surfaces and exports them for viewer presentation.
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

### Requirement: Compositor Capture Publishes Gameplay Metrics

The compositor capture path SHALL publish the timing data required for the Application performance
panel to report `Game FPS` and `Compositor Latency`.

`Game FPS` SHALL be derived from presents or commits for the currently captured game surface only.
`Compositor Latency` SHALL be derived from the interval between an eligible active-surface commit
and the corresponding compositor capture publication.

#### Scenario: Active surface commit updates Game FPS source
- **GIVEN** a game surface is the current capture target
- **WHEN** that surface produces an eligible commit for capture
- **THEN** the compositor capture path SHALL update the `Game FPS` metric source from that event

#### Scenario: Non-target surface does not change Game FPS source
- **GIVEN** a different surface is not the current capture target
- **WHEN** that non-target surface commits
- **THEN** the compositor capture path SHALL NOT count that event toward `Game FPS`

#### Scenario: Commit-to-capture latency is published
- **GIVEN** an eligible active-surface commit produces a captured frame
- **WHEN** the compositor publishes the captured frame for viewer consumption
- **THEN** the compositor capture path SHALL publish `Compositor Latency` for that commit as the
  elapsed commit-to-capture interval
