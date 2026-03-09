## MODIFIED Requirements

### Requirement: Downsample Filter Type Selection

The DownsamplePass SHALL support runtime selection of downsampling filter algorithm via the shader parameter interface.

#### Scenario: Area filter (default)

- **GIVEN** DownsamplePass with `filter_type = 0`
- **WHEN** downsampling is performed
- **THEN** weighted box filter SHALL be used
- **AND** each source pixel SHALL be weighted by coverage overlap

#### Scenario: Gaussian filter

- **GIVEN** DownsamplePass with `filter_type = 1`
- **WHEN** downsampling is performed
- **THEN** Gaussian-weighted bilinear sampling SHALL be used
- **AND** 4 bilinear taps SHALL approximate a Gaussian kernel
- **AND** effective sampling SHALL cover 16 source texels

#### Scenario: Nearest-neighbor filter

- **GIVEN** DownsamplePass with `filter_type = 2`
- **WHEN** downsampling is performed
- **THEN** nearest-neighbor sampling SHALL be used
- **AND** each output pixel SHALL sample a single nearest source texel without area or gaussian weighting

#### Scenario: Filter type exposed as parameter

- **GIVEN** a DownsamplePass instance
- **WHEN** `get_shader_parameters()` is called
- **THEN** a parameter named `filter_type` SHALL be returned
- **AND** min SHALL be 0, max SHALL be 2, step SHALL be 1

#### Scenario: Filter type runtime change

- **GIVEN** DownsamplePass is actively rendering
- **WHEN** `set_shader_parameter("filter_type", 2.0)` is called
- **THEN** the next frame SHALL use nearest-neighbor filter
- **AND** no pipeline rebuild SHALL occur

#### Scenario: Legacy filter values remain stable

- **GIVEN** persisted runtime state or configuration stores `filter_type = 0` or `filter_type = 1`
- **WHEN** that state is loaded by a build that supports nearest-neighbor downsampling
- **THEN** `0` SHALL continue to mean area filtering
- **AND** `1` SHALL continue to mean gaussian filtering

#### Scenario: Persisted nearest-neighbor value remains explicit

- **GIVEN** persisted runtime state or configuration stores `filter_type = 2`
- **WHEN** that state is loaded by a build that supports nearest-neighbor downsampling
- **THEN** `2` SHALL select nearest-neighbor filtering
- **AND** the loaded runtime state SHALL NOT reinterpret `2` as area or gaussian filtering

### Requirement: Downsample Pass

The internal pass library SHALL include a configurable downsampling pass that can be added to the pre-chain.

#### Scenario: Area filter downsampling

- **GIVEN** source image at `1920x1080` and target resolution `640x480`
- **WHEN** downsample pass executes with `filter_type = 0`
- **THEN** each output pixel SHALL be computed as a weighted average of covered source pixels
- **AND** the result SHALL exhibit minimal aliasing compared to point sampling

#### Scenario: Nearest-neighbor downsampling

- **GIVEN** source image at `1920x1080` and target resolution `640x480`
- **WHEN** downsample pass executes with `filter_type = 2`
- **THEN** each output pixel SHALL be produced from nearest-neighbor sampling of the source image
- **AND** the result SHALL preserve sharp pixel edges instead of area-averaged smoothing

#### Scenario: Downsample added to pre-chain when configured

- **GIVEN** source resolution is configured via `--app-width` and/or `--app-height`
- **WHEN** `FilterChain` is created
- **THEN** a `DownsamplePass` SHALL be added to `m_prechain_passes`
- **AND** a framebuffer sized to target resolution SHALL be added to `m_prechain_framebuffers`

#### Scenario: Identity passthrough at same resolution

- **GIVEN** source and target resolution are identical
- **WHEN** downsample pass executes with any supported `filter_type`
- **THEN** output SHALL exactly match input
- **AND** no blurring or aliasing SHALL occur
