# render-pipeline Delta

## ADDED Requirements

### Requirement: Aspect Ratio Display Modes

The output pass SHALL support four display modes for scaling captured frames to the output window, controlled by configuration.

#### Scenario: Fit mode scales image to fit within window

- **GIVEN** scale mode is set to `fit`
- **AND** source image has aspect ratio different from window
- **WHEN** `OutputPass::record()` renders the frame
- **THEN** the viewport SHALL be calculated to show the entire image
- **AND** the image SHALL be centered in the window
- **AND** letterbox (horizontal bars) or pillarbox (vertical bars) SHALL fill unused areas with black

#### Scenario: Fill mode scales image to cover entire window

- **GIVEN** scale mode is set to `fill`
- **AND** source image has aspect ratio different from window
- **WHEN** `OutputPass::record()` renders the frame
- **THEN** the viewport SHALL be calculated to cover the entire window
- **AND** the image SHALL be centered
- **AND** portions of the image extending beyond window bounds SHALL be clipped by scissor

#### Scenario: Stretch mode matches window dimensions exactly

- **GIVEN** scale mode is set to `stretch`
- **WHEN** `OutputPass::record()` renders the frame
- **THEN** the viewport SHALL cover the entire window
- **AND** the image SHALL be scaled to match window dimensions exactly
- **AND** aspect ratio distortion is acceptable

#### Scenario: Integer mode with auto scale finds maximum fit

- **GIVEN** scale mode is set to `integer`
- **AND** integer_scale is `0` (auto)
- **AND** source is 640x480 and window is 1920x1080
- **WHEN** `OutputPass::record()` renders the frame
- **THEN** the maximum integer scale that fits SHALL be calculated (2x = 1280x960)
- **AND** the image SHALL be centered with black borders

#### Scenario: Integer mode with fixed scale of 1 shows original size

- **GIVEN** scale mode is set to `integer`
- **AND** integer_scale is `1`
- **AND** source is 640x480 and window is 1920x1080
- **WHEN** `OutputPass::record()` renders the frame
- **THEN** the viewport SHALL be 640x480 (original size)
- **AND** the image SHALL be centered with black borders

#### Scenario: Integer mode with fixed scale multiplies source dimensions

- **GIVEN** scale mode is set to `integer`
- **AND** integer_scale is `3`
- **AND** source is 640x480
- **WHEN** `OutputPass::record()` renders the frame
- **THEN** the viewport SHALL be 1920x1440 (3x source)
- **AND** portions exceeding window bounds SHALL be clipped by scissor

#### Scenario: Same aspect ratio produces identical output for fit/fill/stretch

- **GIVEN** source image and window have the same aspect ratio
- **WHEN** fit, fill, or stretch mode is used
- **THEN** the output SHALL be identical regardless of mode
- **AND** the image SHALL fill the entire window

### Requirement: Scale Mode Configuration

The application config SHALL include a setting to control the display scale mode.

#### Scenario: Config field definition

- **GIVEN** the `goggles::Config` struct
- **WHEN** `Config::Render` is defined
- **THEN** it SHALL include `ScaleMode scale_mode` field
- **AND** the default value SHALL be `ScaleMode::Stretch`

#### Scenario: TOML parsing for fit mode

- **GIVEN** `goggles.toml` contains `[render] scale_mode = "fit"`
- **WHEN** `load_config()` is called
- **THEN** `config.render.scale_mode` SHALL be `ScaleMode::Fit`

#### Scenario: TOML parsing for fill mode

- **GIVEN** `goggles.toml` contains `[render] scale_mode = "fill"`
- **WHEN** `load_config()` is called
- **THEN** `config.render.scale_mode` SHALL be `ScaleMode::Fill`

#### Scenario: TOML parsing for stretch mode

- **GIVEN** `goggles.toml` contains `[render] scale_mode = "stretch"`
- **WHEN** `load_config()` is called
- **THEN** `config.render.scale_mode` SHALL be `ScaleMode::Stretch`

#### Scenario: TOML parsing for integer mode

- **GIVEN** `goggles.toml` contains `[render] scale_mode = "integer"`
- **WHEN** `load_config()` is called
- **THEN** `config.render.scale_mode` SHALL be `ScaleMode::Integer`

#### Scenario: Missing config field uses default

- **GIVEN** `goggles.toml` does not contain `scale_mode` field
- **WHEN** `load_config()` is called
- **THEN** `config.render.scale_mode` SHALL default to `ScaleMode::Stretch`

#### Scenario: Invalid config value produces error

- **GIVEN** `goggles.toml` contains `[render] scale_mode = "invalid_value"`
- **WHEN** `load_config()` is called
- **THEN** an error SHALL be returned
- **AND** the error message SHALL indicate the invalid value

### Requirement: Integer Scale Configuration

The application config SHALL include a setting to control the integer scaling multiplier when scale_mode is "integer".

#### Scenario: Integer scale field definition

- **GIVEN** the `goggles::Config` struct
- **WHEN** `Config::Render` is defined
- **THEN** it SHALL include `uint32_t integer_scale` field
- **AND** the default value SHALL be `0` (auto)

#### Scenario: Integer scale only applies in integer mode

- **GIVEN** `goggles.toml` contains `[render] scale_mode = "stretch"` and `integer_scale = 2`
- **WHEN** rendering occurs
- **THEN** the `integer_scale` value SHALL be ignored
- **AND** stretch mode behavior SHALL apply

#### Scenario: TOML parsing for auto integer scale

- **GIVEN** `goggles.toml` contains `[render] integer_scale = 0`
- **WHEN** `load_config()` is called
- **THEN** `config.render.integer_scale` SHALL be `0`

#### Scenario: TOML parsing for fixed integer scale

- **GIVEN** `goggles.toml` contains `[render] integer_scale = 3`
- **WHEN** `load_config()` is called
- **THEN** `config.render.integer_scale` SHALL be `3`

#### Scenario: Integer scale validation

- **GIVEN** `goggles.toml` contains `[render] integer_scale = 10`
- **WHEN** `load_config()` is called
- **THEN** an error SHALL be returned
- **AND** the error message SHALL indicate valid range is 0-8

### Requirement: FinalViewportSize Calculation

The filter chain SHALL calculate `FinalViewportSize` based on the scale mode to ensure correct shader behavior when `scale_type = viewport` is used.

#### Scenario: Stretch mode uses swapchain size

- **GIVEN** scale mode is `stretch`
- **AND** swapchain size is 1920x1080
- **WHEN** `FinalViewportSize` is calculated
- **THEN** it SHALL be (1920, 1080)

#### Scenario: Fit mode uses letterboxed effective area

- **GIVEN** scale mode is `fit`
- **AND** swapchain size is 1920x1080 (16:9)
- **AND** source aspect ratio is 4:3
- **WHEN** `FinalViewportSize` is calculated
- **THEN** it SHALL be (1440, 1080) representing the effective content area
- **AND** shaders using `scale_type = viewport` SHALL render at this resolution

#### Scenario: Fill mode uses scaled area exceeding bounds

- **GIVEN** scale mode is `fill`
- **AND** swapchain size is 1920x1080 (16:9)
- **AND** source aspect ratio is 4:3
- **WHEN** `FinalViewportSize` is calculated
- **THEN** it SHALL be (1920, 1440) representing the full scaled content
- **AND** the OutputPass scissor SHALL clip to swapchain bounds

#### Scenario: Integer mode uses source multiplied by scale factor

- **GIVEN** scale mode is `integer`
- **AND** integer_scale is `2`
- **AND** source is 640x480
- **WHEN** `FinalViewportSize` is calculated
- **THEN** it SHALL be (1280, 960)
- **AND** shaders using `scale_type = viewport` SHALL render at this resolution

#### Scenario: Integer mode auto calculates max scale

- **GIVEN** scale mode is `integer`
- **AND** integer_scale is `0` (auto)
- **AND** source is 640x480 and swapchain is 1920x1080
- **WHEN** `FinalViewportSize` is calculated
- **THEN** max scale SHALL be min(floor(1920/640), floor(1080/480)) = min(3, 2) = 2
- **AND** FinalViewportSize SHALL be (1280, 960)

#### Scenario: SemanticBinder uses calculated FinalViewportSize

- **GIVEN** a shader pass with `FinalViewportSize` semantic
- **WHEN** SemanticBinder populates push constants
- **THEN** `FinalViewportSize` SHALL reflect the calculated value based on scale mode
- **AND** NOT the raw swapchain dimensions (except in stretch mode)

### Requirement: Viewport Calculation Utility

The render subsystem SHALL provide a utility function to calculate scaled viewport parameters.

#### Scenario: Calculate fit viewport

- **GIVEN** source extent (640, 480) and target extent (1920, 1080)
- **WHEN** `calculate_viewport()` is called with `ScaleMode::Fit`
- **THEN** result SHALL have width=1440, height=1080
- **AND** offset_x=240, offset_y=0 (centered horizontally)

#### Scenario: Calculate fill viewport

- **GIVEN** source extent (640, 480) and target extent (1920, 1080)
- **WHEN** `calculate_viewport()` is called with `ScaleMode::Fill`
- **THEN** result SHALL have width=1920, height=1440
- **AND** offset_x=0, offset_y=-180 (centered, extends beyond bounds)

#### Scenario: Calculate stretch viewport

- **GIVEN** source extent (640, 480) and target extent (1920, 1080)
- **WHEN** `calculate_viewport()` is called with `ScaleMode::Stretch`
- **THEN** result SHALL have width=1920, height=1080
- **AND** offset_x=0, offset_y=0

#### Scenario: Calculate integer viewport with auto scale

- **GIVEN** source extent (640, 480) and target extent (1920, 1080)
- **WHEN** `calculate_viewport()` is called with `ScaleMode::Integer` and integer_scale=0
- **THEN** result SHALL have width=1280, height=960 (2x)
- **AND** offset_x=320, offset_y=60 (centered)

#### Scenario: Calculate integer viewport with fixed scale

- **GIVEN** source extent (640, 480) and target extent (1920, 1080)
- **WHEN** `calculate_viewport()` is called with `ScaleMode::Integer` and integer_scale=1
- **THEN** result SHALL have width=640, height=480
- **AND** offset_x=640, offset_y=300 (centered)

### Requirement: PassContext Source Extent

The PassContext struct SHALL include source image dimensions to enable aspect ratio calculations.

#### Scenario: Source extent available for aspect ratio calculation

- **GIVEN** a captured frame with known dimensions
- **WHEN** PassContext is created for OutputPass
- **THEN** `source_extent` SHALL contain the width and height of the source image
- **AND** the values SHALL be used for aspect ratio mode calculations
