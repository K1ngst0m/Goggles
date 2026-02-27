## ADDED Requirements

### Requirement: Solid color test client
The system SHALL provide a `solid_color_client` binary that connects to a Wayland compositor and renders a single solid color surface via `wl_shm`.

#### Scenario: Default color rendering
- **GIVEN** `solid_color_client` is launched with `WAYLAND_DISPLAY` set to the compositor socket
- **WHEN** the client connects and commits its first buffer
- **THEN** the surface SHALL be filled with RGBA(255, 0, 0, 255) by default

#### Scenario: Color override via environment variable
- **GIVEN** `TEST_COLOR=0,255,0,255` is set in the environment
- **WHEN** `solid_color_client` renders its surface
- **THEN** every pixel SHALL be RGBA(0, 255, 0, 255)

#### Scenario: Clean exit after frame count
- **GIVEN** `solid_color_client` has committed its buffers
- **WHEN** it has rendered at least 30 stable frames (default)
- **THEN** the process SHALL exit with code 0

### Requirement: Quadrant test client
The system SHALL provide a `quadrant_client` binary that renders four colored quadrants at deterministic pixel positions.

#### Scenario: Quadrant color layout
- **GIVEN** `quadrant_client` is launched and connected to the compositor
- **WHEN** it commits its first buffer
- **THEN** the top-left quadrant SHALL be red (255, 0, 0, 255)
- **AND** the top-right quadrant SHALL be green (0, 255, 0, 255)
- **AND** the bottom-left quadrant SHALL be blue (0, 0, 255, 255)
- **AND** the bottom-right quadrant SHALL be white (255, 255, 255, 255)

#### Scenario: Quadrant boundary precision
- **GIVEN** a surface of width W and height H
- **WHEN** quadrant colors are rendered
- **THEN** the pixel at (W/2 - 1, H/2 - 1) SHALL be red
- **AND** the pixel at (W/2 + 1, H/2 - 1) SHALL be green
- **AND** the pixel at (W/2 - 1, H/2 + 1) SHALL be blue
- **AND** the pixel at (W/2 + 1, H/2 + 1) SHALL be white

#### Scenario: Clean exit after frame count
- **GIVEN** `quadrant_client` has committed its buffers
- **WHEN** it has rendered at least 30 stable frames
- **THEN** the process SHALL exit with code 0

### Requirement: Gradient test client
The system SHALL provide a `gradient_client` binary that renders a horizontal linear gradient from black (left) to white (right) via `wl_shm`.

#### Scenario: Gradient pixel values
- **GIVEN** `gradient_client` is connected and has committed its buffer
- **WHEN** the output PNG is read
- **THEN** the pixel at x=0 SHALL have R=G=B=0 (black)
- **AND** the pixel at x=W-1 SHALL have R=G=B=255 (white)
- **AND** intermediate pixels SHALL increase monotonically left to right

#### Scenario: Clean exit after frame count
- **GIVEN** `gradient_client` has rendered at least 30 stable frames
- **THEN** the process SHALL exit with code 0

### Requirement: Multi-surface test client
The system SHALL provide a `multi_surface_client` binary that creates a main surface and one `wl_subsurface` with distinct solid colors.

#### Scenario: Two-surface layout
- **GIVEN** `multi_surface_client` is connected and both surfaces are committed
- **WHEN** the compositor presents the scene
- **THEN** the main surface SHALL render its background color (blue: 0, 0, 255, 255)
- **AND** the subsurface SHALL render its foreground color (red: 255, 0, 0, 255)
- **AND** the subsurface SHALL be composited on top of the main surface at a known parent-relative offset set via `wl_subsurface.set_position()`

#### Scenario: Clean exit after frame count
- **GIVEN** both surfaces have been committed for at least 30 frames
- **THEN** the process SHALL exit with code 0

### Requirement: Wayland protocol compliance
All test clients SHALL use `wl_shm` shared memory buffers and comply with the `xdg-wm-base` shell protocol for toplevel surface creation.

#### Scenario: No Vulkan dependency
- **WHEN** any test client binary is executed in an environment without a GPU or Vulkan loader
- **THEN** it SHALL connect, render, and exit successfully using only `wl_shm`

#### Scenario: WAYLAND_DISPLAY is required
- **GIVEN** `WAYLAND_DISPLAY` is not set or the socket does not exist
- **WHEN** any test client is launched
- **THEN** the process SHALL exit with a non-zero code and print an error to stderr
