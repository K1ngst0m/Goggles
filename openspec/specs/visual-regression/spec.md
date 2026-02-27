# visual-regression Specification

## Purpose
Defines requirements for the image comparison library, CLI tool, and headless pipeline smoke test that form the visual regression testing foundation for Goggles. These capabilities were introduced in the archived change `test-framework-phase1` and are the basis for Phase 2 visual regression tests.
## Requirements
### Requirement: Image comparison library
The system SHALL provide a C++ image comparison library at `tests/visual/image_compare.hpp` that compares two PNG images with configurable per-channel tolerance.

#### Scenario: Identical images pass
- **GIVEN** two PNG files with identical pixel data
- **WHEN** `compare_images(actual_path, reference_path, tolerance)` is called with any tolerance ≥ 0
- **THEN** `CompareResult.passed` SHALL be `true`
- **AND** `CompareResult.max_channel_diff` SHALL be `0.0`
- **AND** `CompareResult.failing_pixels` SHALL be `0`

#### Scenario: Differing images fail at zero tolerance
- **GIVEN** two PNG files where at least one pixel differs by 1 channel value
- **WHEN** `compare_images()` is called with `tolerance = 0.0`
- **THEN** `CompareResult.passed` SHALL be `false`
- **AND** `CompareResult.failing_pixels` SHALL be ≥ 1

#### Scenario: Tolerance allows small differences
- **GIVEN** two PNG files where all pixels differ by at most 2/255 per channel
- **WHEN** `compare_images()` is called with `tolerance = 2.0/255.0`
- **THEN** `CompareResult.passed` SHALL be `true`

#### Scenario: CompareResult fields are populated
- **GIVEN** a comparison that produces failures
- **WHEN** `compare_images()` returns
- **THEN** `CompareResult.max_channel_diff` SHALL be the maximum per-channel delta across all pixels (normalized 0.0–1.0)
- **AND** `CompareResult.mean_diff` SHALL be the mean per-pixel average-channel delta
- **AND** `CompareResult.failing_percentage` SHALL equal `failing_pixels / (width * height)` × 100

#### Scenario: Diff image is generated on failure
- **GIVEN** a comparison that fails AND `diff_output_path` is provided
- **WHEN** `compare_images()` returns
- **THEN** a PNG SHALL be written at `diff_output_path`
- **AND** pixels that exceeded tolerance SHALL be highlighted in red (255, 0, 0, 255) in the diff image
- **AND** passing pixels SHALL be shown at reduced intensity (≤ 25% of original value)

#### Scenario: Size mismatch is a failure
- **GIVEN** two PNG files with different dimensions
- **WHEN** `compare_images()` is called
- **THEN** `CompareResult.passed` SHALL be `false`
- **AND** an error message describing the dimension mismatch SHALL be set

### Requirement: Image comparison CLI tool
The system SHALL provide a `goggles_image_compare` CLI binary that wraps the comparison library for use in shell scripts and pixi tasks.

#### Scenario: Pass exit code
- **WHEN** `goggles_image_compare actual.png reference.png --tolerance 0.01` is run and images match within tolerance
- **THEN** the process SHALL exit with code 0

#### Scenario: Fail exit code
- **WHEN** `goggles_image_compare actual.png reference.png --tolerance 0.0` is run and images differ
- **THEN** the process SHALL exit with code 1
- **AND** a summary SHALL be printed to stdout including `failing_pixels` and `max_channel_diff`

#### Scenario: Diff image output
- **WHEN** `--diff diff.png` is passed and the comparison fails
- **THEN** a diff PNG SHALL be written to `diff.png`

#### Scenario: Missing file error
- **WHEN** either `actual.png` or `reference.png` does not exist
- **THEN** the process SHALL exit with code 2 and print an error to stderr

### Requirement: Headless pipeline smoke test
The system SHALL provide a CTest integration test that exercises the full `Application::create_headless()` → filter chain → `readback_to_png()` pipeline.

#### Scenario: Smoke test produces a valid PNG
- **GIVEN** the project is built (includes `goggles` binary and `solid_color_client`)
- **WHEN** CTest runs the headless smoke test
- **THEN** `goggles --headless --frames 5 --output <tmp>/smoke.png -- solid_color_client` SHALL exit with code 0
- **AND** `<tmp>/smoke.png` SHALL exist and be a non-empty file

#### Scenario: Smoke test is labeled integration
- **GIVEN** any CTest configuration
- **WHEN** `ctest -L integration` is run
- **THEN** the headless smoke test SHALL be included in the run

#### Scenario: Smoke test is excluded from unit tier
- **WHEN** `ctest -L unit` is run
- **THEN** the headless smoke test SHALL NOT be included

