## ADDED Requirements

### Requirement: Pre-Chain Stage Infrastructure

The filter chain SHALL support an optional pre-chain stage that processes captured frames before the RetroArch shader passes. Pre-chain passes use the same infrastructure as filter passes (Framebuffer, shader compilation) for code reuse.

#### Scenario: Pre-chain disabled by default

- **GIVEN** no source resolution is configured via CLI
- **WHEN** `FilterChain` is created
- **THEN** no pre-chain passes SHALL be created
- **AND** captured frames SHALL pass directly to RetroArch passes (or OutputPass in passthrough mode)

#### Scenario: Pre-chain enabled when source resolution configured

- **GIVEN** source resolution is configured via `--app-width` and `--app-height`
- **WHEN** `FilterChain` is created
- **THEN** a pre-chain downsample pass SHALL be initialized
- **AND** the pre-chain framebuffer SHALL be sized to the configured resolution

#### Scenario: Pre-chain output becomes Original for RetroArch chain

- **GIVEN** pre-chain stage is active
- **WHEN** `FilterChain::record()` executes
- **THEN** pre-chain passes SHALL execute first
- **AND** the pre-chain output SHALL be used as `original_view` for RetroArch passes
- **AND** `OriginalSize` semantic SHALL reflect pre-chain output dimensions

### Requirement: Area Downsample Shader

The internal shader library SHALL include an area filter downsampling shader for high-quality resolution reduction.

#### Scenario: Area filter downsampling

- **GIVEN** source image at 1920x1080 and target resolution 640x480
- **WHEN** downsample shader executes
- **THEN** each output pixel SHALL be computed as a weighted average of covered source pixels
- **AND** the result SHALL exhibit minimal aliasing compared to point sampling

#### Scenario: Shader uses push constants for dimensions

- **GIVEN** the downsample shader
- **WHEN** `DownsamplePass` records commands
- **THEN** source and target dimensions SHALL be passed via push constants
- **AND** the shader SHALL compute sample weights dynamically

#### Scenario: Identity passthrough at same resolution

- **GIVEN** source and target resolution are identical
- **WHEN** downsample shader executes
- **THEN** output SHALL exactly match input
- **AND** no blurring or aliasing SHALL occur

### Requirement: Source Resolution CLI Semantics

The `--app-width` and `--app-height` CLI options SHALL set the source resolution for the filter chain input, independent of capture mode.

#### Scenario: Options set pre-chain resolution

- **GIVEN** user specifies `--app-width 640 --app-height 480`
- **WHEN** Goggles starts
- **THEN** `FilterChain` SHALL be configured with source resolution 640x480
- **AND** pre-chain downsample pass SHALL be created

#### Scenario: Options work without WSI proxy

- **GIVEN** user specifies `--app-width 320 --app-height 240` without `--wsi-proxy`
- **WHEN** Goggles captures frames from target app
- **THEN** captured frames at native resolution SHALL be downsampled to 320x240
- **AND** the 320x240 image SHALL be the input to RetroArch shader passes

#### Scenario: Options still set environment variables

- **GIVEN** user specifies `--app-width` and `--app-height`
- **WHEN** target app is launched
- **THEN** `GOGGLES_WIDTH` and `GOGGLES_HEIGHT` environment variables SHALL still be set
- **AND** WSI proxy (if enabled) SHALL use these values for virtual surface sizing
