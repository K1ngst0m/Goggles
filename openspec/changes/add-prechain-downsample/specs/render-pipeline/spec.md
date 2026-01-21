## ADDED Requirements

### Requirement: Pre-Chain Stage Infrastructure

The filter chain SHALL support a generic pre-chain stage that processes captured frames before the RetroArch shader passes. The pre-chain is a vector of passes, analogous to the RetroArch pass vector, allowing multiple preprocessing steps.

#### Scenario: Pre-chain as extensible pass vector

- **GIVEN** `FilterChain` is initialized
- **WHEN** pre-chain passes are configured
- **THEN** `m_prechain_passes` SHALL be a vector capable of holding multiple passes
- **AND** `m_prechain_framebuffers` SHALL be a vector of corresponding framebuffers
- **AND** passes SHALL execute in vector order

#### Scenario: Pre-chain disabled by default

- **GIVEN** no pre-chain passes are configured
- **WHEN** `FilterChain::record()` executes
- **THEN** captured frames SHALL pass directly to RetroArch passes (or OutputPass in passthrough mode)

#### Scenario: Pre-chain output becomes Original for RetroArch chain

- **GIVEN** pre-chain contains one or more passes
- **WHEN** `FilterChain::record()` executes
- **THEN** pre-chain passes SHALL execute first in vector order
- **AND** the final pre-chain output SHALL be used as `original_view` for RetroArch passes
- **AND** `OriginalSize` semantic SHALL reflect final pre-chain output dimensions

#### Scenario: Generic pre-chain recording

- **GIVEN** pre-chain contains N passes
- **WHEN** `record_prechain()` executes
- **THEN** each pass SHALL receive the previous pass's output as input
- **AND** image barriers SHALL be inserted between passes
- **AND** the loop SHALL NOT be hardcoded to a specific pass type

### Requirement: Downsample Pass

The internal pass library SHALL include an area-filter downsampling pass that can be added to the pre-chain.

#### Scenario: Area filter downsampling

- **GIVEN** source image at 1920x1080 and target resolution 640x480
- **WHEN** downsample pass executes
- **THEN** each output pixel SHALL be computed as a weighted average of covered source pixels
- **AND** the result SHALL exhibit minimal aliasing compared to point sampling

#### Scenario: Downsample added to pre-chain when configured

- **GIVEN** source resolution is configured via `--app-width` and/or `--app-height`
- **WHEN** `FilterChain` is created
- **THEN** a `DownsamplePass` SHALL be added to `m_prechain_passes`
- **AND** a framebuffer sized to target resolution SHALL be added to `m_prechain_framebuffers`

#### Scenario: Identity passthrough at same resolution

- **GIVEN** source and target resolution are identical
- **WHEN** downsample pass executes
- **THEN** output SHALL exactly match input
- **AND** no blurring or aliasing SHALL occur

### Requirement: Source Resolution CLI Semantics

The `--app-width` and `--app-height` CLI options SHALL configure the downsample pass in the pre-chain. Either option may be specified alone, with the other dimension calculated to preserve aspect ratio.

#### Scenario: Both dimensions specified

- **GIVEN** user specifies `--app-width 640 --app-height 480`
- **WHEN** Goggles starts
- **THEN** `DownsamplePass` SHALL be added to pre-chain with target 640x480

#### Scenario: Only width specified preserves aspect ratio

- **GIVEN** user specifies `--app-width 640` without `--app-height`
- **AND** captured frame is 1920x1080 (16:9 aspect ratio)
- **WHEN** first frame is processed
- **THEN** height SHALL be computed as `round(640 * 1080 / 1920) = 360`
- **AND** downsample pass target SHALL be 640x360

#### Scenario: Only height specified preserves aspect ratio

- **GIVEN** user specifies `--app-height 480` without `--app-width`
- **AND** captured frame is 1920x1080 (16:9 aspect ratio)
- **WHEN** first frame is processed
- **THEN** width SHALL be computed as `round(480 * 1920 / 1080) = 853`
- **AND** downsample pass target SHALL be 853x480

#### Scenario: Options still set environment variables

- **GIVEN** user specifies `--app-width` and/or `--app-height`
- **WHEN** target app is launched
- **THEN** `GOGGLES_WIDTH` and `GOGGLES_HEIGHT` environment variables SHALL be set for specified dimensions
- **AND** WSI proxy (if enabled) SHALL use these values for virtual surface sizing
