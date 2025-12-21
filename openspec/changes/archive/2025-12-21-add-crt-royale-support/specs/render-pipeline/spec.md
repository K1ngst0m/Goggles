## ADDED Requirements

### Requirement: Preset Texture Assets

The filter chain SHALL load external textures listed in a RetroArch preset `textures` entry and bind them by name to matching sampler uniforms.

#### Scenario: Mask LUTs loaded and bound
- **WHEN** a preset defining `textures = "mask_a;mask_b"` with paths for each name is loaded
- **THEN** each texture SHALL be decoded and uploaded to a GPU image
- **AND** each texture SHALL be bound to the sampler with the same name in the shader

### Requirement: Preset Texture Sampling Overrides

The filter chain SHALL honor per-texture sampling flags from the preset (`*_linear`, `*_mipmap`, `*_wrap_mode`).

#### Scenario: Repeat + mipmapped mask texture
- **GIVEN** a preset sets `mask_grille_texture_large_wrap_mode = "repeat"` and `mask_grille_texture_large_mipmap = true`
- **WHEN** the preset is loaded
- **THEN** the bound sampler SHALL use repeat addressing and mipmapped sampling

### Requirement: Alias Pass Routing

The filter chain SHALL expose aliased pass outputs as named textures for subsequent passes, and SHALL provide `ALIASSize` push constants for aliased inputs.

#### Scenario: Vertical scanline alias
- **GIVEN** pass 1 declares `alias1 = "VERTICAL_SCANLINES"`
- **WHEN** pass 7 samples a sampler named `VERTICAL_SCANLINES`
- **THEN** the bound image SHALL be the output of pass 1
- **AND** `VERTICAL_SCANLINESSize` SHALL reflect the aliased texture size as vec4

### Requirement: Parameter Override Binding

The filter chain SHALL apply preset parameter overrides and populate shader parameters by name into push constants or UBO members.

#### Scenario: Override applied to UBO member
- **GIVEN** a shader defines parameter `mask_type` in its UBO
- **AND** the preset includes `mask_type = 2.0`
- **WHEN** the pass is recorded
- **THEN** the UBO member named `mask_type` SHALL be written with value `2.0`

### Requirement: Pass Input Mipmap Control

The filter chain SHALL honor `mipmap_inputN` when selecting sampler state for a pass input.

#### Scenario: Mipmap input enabled
- **GIVEN** a preset sets `mipmap_input11 = true`
- **WHEN** pass 11 samples `Source`
- **THEN** the sampler bound to `Source` SHALL have mipmapping enabled
