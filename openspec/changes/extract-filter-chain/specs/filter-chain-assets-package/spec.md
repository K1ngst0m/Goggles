# Delta for filter-chain-assets-package

## MODIFIED Requirements

### Requirement: Curated Upstream Shader Content

(Previously: The standalone asset package SHALL include only the specific upstream shader files referenced by zfast integration tests and shader validation tests, and SHALL NOT mirror the full `shaders/retroarch/` upstream collection.)

The standalone asset package SHALL retain exactly one upstream shader preset: `crt-lottes-fast`. All other upstream presets from the RetroArch shader collection SHALL be removed from the standalone repository. Test/diagnostic shaders and internal blit/downsample shaders SHALL be retained regardless of upstream origin.

#### Scenario: Only crt-lottes-fast retained from upstream

- **GIVEN** the standalone filter-chain asset directory `assets/shaders/upstream/`
- **WHEN** preset files (`.slangp`) in the upstream directory are enumerated
- **THEN** exactly one upstream preset SHALL be present: `crt-lottes-fast`
- **AND** no other upstream presets (e.g., full CRT, scanline, NTSC variants) SHALL be present

#### Scenario: Test and diagnostic shaders retained

- **GIVEN** the standalone filter-chain asset directory
- **WHEN** test shader fixtures and diagnostic shaders are inspected
- **THEN** all test shaders used by contract tests (format handling, history, feedback, frame count, pragma parsing, format decoding) SHALL remain present
- **AND** diagnostic shader presets used by diagnostics unit tests SHALL remain present

#### Scenario: Internal shaders retained

- **GIVEN** the standalone filter-chain asset directory
- **WHEN** internal shader content is inspected
- **THEN** blit shaders and downsample shaders used by the filter-chain pipeline SHALL remain present
- **AND** their presence SHALL NOT depend on upstream shader selection

#### Scenario: Embedded asset binary contains expected content

- **GIVEN** the standalone filter-chain library is built with embedded assets
- **WHEN** the embedded asset binary content is inspected or validated at runtime
- **THEN** the binary SHALL contain `crt-lottes-fast` preset data, all test/diagnostic shader data, and all internal shader data
- **AND** the binary SHALL NOT contain data from removed upstream presets
