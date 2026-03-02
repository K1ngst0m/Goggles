# visual-regression Specification

## Purpose
Defines requirements for the image comparison library, CLI tool, headless pipeline smoke test, aspect ratio visual tests, shader visual tests, and golden image workflow that form the visual regression testing foundation for Goggles. Phase 1 established the infrastructure (`test-framework-phase1`); Phase 2 introduced the first batch of automated visual tests (`test-framework-phase2`).
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

### Requirement: Aspect ratio visual tests
The system SHALL provide 8 Catch2 visual tests in `tests/visual/test_aspect_ratio.cpp` that assert correct pixel-region geometry for each scale mode using `quadrant_client` as the fixed 640×480 source.

#### Scenario: fit — source narrower than viewport (letterbox)
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "fit"` and a 640×480 source
- **WHEN** the output PNG is captured
- **THEN** pixels at x<240 and x≥1680 SHALL be black (pillarbox side bars)
- **AND** the content rectangle [240, 0, 1440, 1080] SHALL contain the correct quadrant colors within `CONTENT_TOLERANCE = 2/255`

#### Scenario: fit — source aspect ratio matches viewport (no bars)
- **GIVEN** goggles runs headless at 800×600 with `scale_mode = "fit"` and a 640×480 source
- **WHEN** the output PNG is captured
- **THEN** no black bars SHALL be present
- **AND** the full 800×600 output SHALL contain the correct quadrant colors

#### Scenario: fill — content overflows viewport
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "fill"` and a 640×480 source
- **WHEN** the output PNG is captured
- **THEN** the center pixel (960, 540) SHALL NOT be black

#### Scenario: stretch — entire viewport covered
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "stretch"` and a 640×480 source
- **WHEN** the output PNG is captured
- **THEN** the full 1920×1080 output SHALL contain the correct quadrant colors with no black bars

#### Scenario: integer scale 1x
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "integer"`, `integer_scale = 1`
- **WHEN** the output PNG is captured
- **THEN** the content rectangle SHALL be 640×480 at offset (640, 300)
- **AND** black border pixels SHALL be present in all four surrounding regions

#### Scenario: integer scale 2x
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "integer"`, `integer_scale = 2`
- **WHEN** the output PNG is captured
- **THEN** the content rectangle SHALL be 1280×960 at offset (320, 60)
- **AND** black border pixels SHALL be present in all four surrounding regions

#### Scenario: integer scale auto
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "integer"`, `integer_scale = 0` (auto)
- **WHEN** the output PNG is captured
- **THEN** auto scale SHALL resolve to 2 (min(1920÷640, 1080÷480) = min(3,2) = 2)
- **AND** geometry SHALL match the integer 2x scenario

#### Scenario: dynamic scale mode
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "dynamic"` and a 640×480 source
- **WHEN** the source resolution is stable (no mid-stream change)
- **THEN** dynamic SHALL fall back to fit behaviour
- **AND** geometry SHALL match the fit letterbox scenario (side bars at x<240, x≥1680)

### Requirement: Shader visual tests with golden image comparison
The system SHALL provide 3 Catch2 visual tests in `tests/visual/test_shader_basic.cpp` that compare rendered output to golden reference images within explicit tolerance contracts.

#### Scenario: bypass shader matches golden
- **GIVEN** golden `tests/golden/shader_bypass_quadrant.png` exists
- **WHEN** goggles runs headless with no shader preset and the output is compared to the golden
- **THEN** the comparison SHALL pass with `tolerance = 2/255` and `max_failing_pct ≤ 0.1%`

#### Scenario: crt-lottes-fast shader matches golden
- **GIVEN** golden `tests/golden/shader_zfast_quadrant.png` exists
- **WHEN** goggles runs headless with `crt-lottes-fast.slangp` and the output is compared to the golden
- **THEN** the comparison SHALL pass with `tolerance = 0.05` and `max_failing_pct ≤ 5.0%`

#### Scenario: filter-chain toggle produces distinct bypass and crt outputs
- **GIVEN** both golden images exist
- **WHEN** goggles is run twice — once with bypass config and once with crt config
- **THEN** the bypass run SHALL match the bypass golden within bypass tolerance
- **AND** the crt run SHALL match the crt golden within crt tolerance

#### Scenario: tests skip gracefully when goldens are absent
- **GIVEN** golden images have not been generated (e.g. fresh checkout without GPU)
- **WHEN** CTest runs the shader visual tests
- **THEN** each test SHALL emit a Catch2 SKIP (not FAIL) with a message directing the user to run `pixi run update-golden`

### Requirement: Golden image update workflow
The system SHALL provide a reproducible mechanism to regenerate golden reference images.

#### Scenario: update-golden script captures both goldens
- **GIVEN** the project is built (`pixi run build` completed)
- **WHEN** `pixi run update-golden` is executed on a machine with a GPU
- **THEN** `tests/golden/shader_bypass_quadrant.png` SHALL be overwritten with the current bypass render
- **AND** `tests/golden/shader_zfast_quadrant.png` SHALL be overwritten with the current zfast render

#### Scenario: golden PNGs are tracked via Git LFS
- **GIVEN** Git LFS is configured in the repository
- **WHEN** `*.png` files are committed to `tests/golden/`
- **THEN** they SHALL be stored as LFS pointers per `tests/golden/.gitattributes`

### Requirement: Visual test CTest label isolation
Visual tests SHALL carry only the `visual` CTest label; they MUST NOT carry `unit` or `integration` labels.

#### Scenario: visual label selects new tests
- **WHEN** `ctest --preset test -L visual` is run
- **THEN** `test_aspect_ratio` and `test_shader_basic` SHALL be included

#### Scenario: unit label excludes visual tests
- **WHEN** `ctest --preset test -L unit` is run
- **THEN** `test_aspect_ratio` and `test_shader_basic` SHALL NOT be included

#### Scenario: integration label excludes visual tests
- **WHEN** `ctest --preset test -L integration` is run
- **THEN** `test_aspect_ratio` and `test_shader_basic` SHALL NOT be included

