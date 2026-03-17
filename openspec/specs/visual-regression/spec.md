# visual-regression Specification

## Purpose
Defines the current Goggles visual regression contract: the image-comparison library and CLI, the headless smoke check, the aspect-ratio and shader-golden tests, the golden refresh workflow, and the labeling/scope boundaries for Goggles-owned visual regression coverage.

## Requirements
### Requirement: Image comparison library
The system MUST provide an image-comparison library with an explicit `Image` model, `load_png(...)` loader, `compare_images(...)` overloads for whole-image and ROI comparison, and `generate_diff_heatmap(...)`. `compare_images(...)` MUST compare already-loaded `Image` values rather than file paths.

#### Scenario: Identical images pass
- **GIVEN** two loaded images with identical pixel data
- **WHEN** `compare_images(actual, reference, tolerance)` is called with any tolerance greater than or equal to `0.0`
- **THEN** `CompareResult.passed` SHALL be `true`
- **AND** `CompareResult.max_channel_diff` SHALL be `0.0`
- **AND** `CompareResult.failing_pixels` SHALL be `0`

#### Scenario: Differing images fail at zero tolerance
- **GIVEN** two loaded images where at least one pixel differs by one channel value
- **WHEN** `compare_images(...)` is called with `tolerance = 0.0`
- **THEN** `CompareResult.passed` SHALL be `false`
- **AND** `CompareResult.failing_pixels` SHALL be at least `1`

#### Scenario: Tolerance allows small differences
- **GIVEN** two loaded images where all channels differ by at most `2/255`
- **WHEN** `compare_images(...)` is called with `tolerance = 2.0/255.0`
- **THEN** `CompareResult.passed` SHALL be `true`

#### Scenario: Result metrics are populated
- **GIVEN** a comparison is performed over either the whole image or an ROI
- **WHEN** `compare_images(...)` returns
- **THEN** `CompareResult` SHALL report `max_channel_diff`, `mean_diff`, `failing_pixels`, and `failing_percentage`
- **AND** `failing_percentage` SHALL be based on the compared pixel count for the selected image region

#### Scenario: Diff image is generated on failure
- **GIVEN** a comparison fails and a diff output path is provided
- **WHEN** `compare_images(...)` returns
- **THEN** a PNG SHALL be written at the diff path
- **AND** pixels that exceeded tolerance SHALL be highlighted in red
- **AND** non-failing pixels SHALL be shown at reduced intensity

#### Scenario: Size mismatch is reported as comparison failure
- **GIVEN** two loaded images with different dimensions
- **WHEN** `compare_images(...)` is called
- **THEN** `CompareResult.passed` SHALL be `false`
- **AND** `CompareResult.error_message` SHALL describe the dimension mismatch

#### Scenario: Structural similarity is optional
- **GIVEN** two loaded images of the same dimensions
- **WHEN** `compare_images(...)` is called with structural similarity enabled
- **THEN** `CompareResult.structural_similarity` SHALL report a value in the range `[0.0, 1.0]`

#### Scenario: ROI comparison scopes the evaluated region
- **GIVEN** two loaded images and a rectangular region of interest
- **WHEN** the ROI overload of `compare_images(...)` is called
- **THEN** comparison metrics SHALL be computed only over the ROI bounds
- **AND** invalid ROI inputs SHALL produce a failed result with an explanatory error message

#### Scenario: Heatmap generation is available as a separate helper
- **GIVEN** two loaded images of the same dimensions
- **WHEN** `generate_diff_heatmap(...)` is called
- **THEN** the helper SHALL write a heatmap PNG representing per-pixel difference magnitude
- **AND** size mismatches SHALL be reported as an error result

### Requirement: Image comparison CLI tool
The system MUST provide a `goggles_image_compare` CLI binary that loads two PNG files, compares them through the image-comparison library, accepts optional `--tolerance` and `--diff` arguments, and uses process exit codes to report success or failure.

#### Scenario: Pass exit code
- **WHEN** `goggles_image_compare actual.png reference.png --tolerance 0.01` is run and the images match within tolerance
- **THEN** the process SHALL exit with code `0`

#### Scenario: Fail exit code and summary output
- **WHEN** `goggles_image_compare actual.png reference.png --tolerance 0.0` is run and the images differ
- **THEN** the process SHALL exit with code `1`
- **AND** stdout SHALL include `failing_pixels`, `max_channel_diff`, and `failing_percentage`

#### Scenario: Usage or load errors return code 2
- **WHEN** required positional arguments are missing, an option value is invalid, or either input PNG fails to load
- **THEN** the process SHALL exit with code `2`

#### Scenario: Diff image output
- **WHEN** `--diff diff.png` is passed and the comparison fails
- **THEN** a diff PNG SHALL be written to `diff.png`

### Requirement: Headless pipeline smoke test
The system MUST provide a headless smoke test that runs Goggles for a small fixed frame count, writes a PNG, and verifies that the output file exists. This smoke coverage MUST remain labeled as `integration` rather than `visual`.

#### Scenario: Smoke test produces a PNG
- **GIVEN** the project is built with the required binaries
- **WHEN** the headless smoke test runs
- **THEN** Goggles SHALL exit successfully after rendering a fixed small number of frames
- **AND** the configured smoke-test PNG output SHALL exist

#### Scenario: Smoke test is integration-labeled
- **WHEN** CTest labels are inspected
- **THEN** the headless smoke test SHALL be in the `integration` tier
- **AND** it SHALL NOT be part of the `visual` label contract

### Requirement: Aspect ratio visual tests
The system MUST provide eight Catch2 visual tests that validate the rendered geometry for fit, fill, stretch, integer-1x, integer-2x, integer-auto, and dynamic scale-mode scenarios using the quadrant client as the deterministic source.

#### Scenario: Fit letterbox geometry
- **GIVEN** headless output at `1920x1080` in fit mode with the fixed `640x480` quadrant source
- **WHEN** the output PNG is captured
- **THEN** side bars SHALL be black at the expected pillarbox positions
- **AND** the content rectangle SHALL show the expected quadrant colors within the configured content tolerance

#### Scenario: Fit perfect 4:3 geometry
- **GIVEN** headless output forced to `800x600` in fit mode with the same quadrant source
- **WHEN** the output PNG is captured
- **THEN** the full output SHALL contain the expected quadrant colors with no black bars

#### Scenario: Fill geometry covers the viewport
- **GIVEN** headless output at `1920x1080` in fill mode
- **WHEN** the output PNG is captured
- **THEN** the viewport center SHALL contain non-black content rather than border fill

#### Scenario: Stretch geometry fills the viewport
- **GIVEN** headless output at `1920x1080` in stretch mode
- **WHEN** the output PNG is captured
- **THEN** the full viewport SHALL contain the expected quadrant colors with no black bars

#### Scenario: Integer scale geometries match their fixed rectangles
- **GIVEN** headless output at `1920x1080` in integer mode with scale `1`, scale `2`, or auto scale
- **WHEN** the output PNG is captured
- **THEN** the content rectangle and black-border regions SHALL match the expected centered geometry for each case
- **AND** auto scale SHALL resolve to the same geometry as `2x` for the fixed quadrant source

#### Scenario: Dynamic mode falls back to fit for stable source size
- **GIVEN** headless output at `1920x1080` in dynamic mode with no mid-stream source-size change
- **WHEN** the output PNG is captured
- **THEN** the geometry SHALL match the fit-letterbox case

### Requirement: Shader visual tests with golden image comparison
The system MUST provide three Catch2 shader visual tests covering bypass, zfast-crt, and a bypass-vs-zfast toggle distinction. These tests MUST compare captured output images against golden PNGs using explicit tolerance and failing-percentage thresholds.

#### Scenario: Bypass shader matches golden
- **GIVEN** the bypass golden image is present
- **WHEN** Goggles renders the bypass shader scenario and compares the output against the golden
- **THEN** the comparison SHALL pass within the bypass tolerance and failing-percentage threshold

#### Scenario: Zfast shader matches golden
- **GIVEN** the zfast-crt golden image is present
- **WHEN** Goggles renders the zfast scenario and compares the output against the golden
- **THEN** the comparison SHALL pass within the zfast tolerance and failing-percentage threshold

#### Scenario: Toggle scenarios remain distinct and valid
- **GIVEN** both shader goldens are present
- **WHEN** Goggles renders the bypass and zfast scenarios separately
- **THEN** each output SHALL match its corresponding golden contract

#### Scenario: Missing goldens skip instead of fail
- **GIVEN** one or more required shader goldens are absent
- **WHEN** the shader visual tests run
- **THEN** the affected tests SHALL emit Catch2 `SKIP` results rather than failures
- **AND** the skip message SHALL direct the user to run `pixi run update-golden`

### Requirement: Golden image update workflow
The system MUST provide a reproducible workflow to refresh Goggles-owned golden PNGs through `pixi run update-golden`. The Goggles visual-regression contract only requires final-output shader goldens for the active visual tests.

#### Scenario: Golden refresh updates the shader goldens
- **GIVEN** the project is built on a machine that can execute the capture workflow
- **WHEN** `pixi run update-golden` is executed
- **THEN** the bypass and zfast shader goldens SHALL be regenerated from current captures

#### Scenario: Golden PNGs remain Git LFS tracked
- **GIVEN** golden PNGs are committed under the golden-image directory
- **WHEN** repository LFS tracking rules are applied
- **THEN** those PNGs SHALL remain tracked through Git LFS

### Requirement: Visual test CTest label isolation
The visual regression executables registered through the visual test CMake wiring MUST carry the `visual` label and MUST NOT also be labeled `unit` or `integration`.

#### Scenario: Visual label selects visual tests
- **WHEN** `ctest --preset test -L visual` is run
- **THEN** the aspect-ratio and shader visual tests SHALL be included

#### Scenario: Unit label excludes visual tests
- **WHEN** `ctest --preset test -L unit` is run
- **THEN** the aspect-ratio and shader visual tests SHALL NOT be included

#### Scenario: Integration label excludes visual tests
- **WHEN** `ctest --preset test -L integration` is run
- **THEN** the aspect-ratio and shader visual tests SHALL NOT be included

### Requirement: Goggles visual-regression scope excludes standalone filter-chain diagnostics workflows
Goggles-owned visual regression requirements SHALL cover final-output image comparison, CLI/library helpers, headless smoke, aspect-ratio tests, shader goldens, and visual CTest labeling. Intermediate-pass golden baselines, earliest-divergence localization, temporal-sequence golden requirements, and semantic-probe preset requirements are OUT OF SCOPE for the current Goggles visual-regression contract.

#### Scenario: Consumer reads the Goggles visual-regression contract
- **GIVEN** a reader wants to know what Goggles visual tests currently guarantee
- **WHEN** it inspects this living spec
- **THEN** it SHALL find requirements for the actively maintained Goggles visual test surface
- **AND** it SHALL NOT interpret standalone filter-chain diagnostic workflows as required Goggles visual-regression behavior
