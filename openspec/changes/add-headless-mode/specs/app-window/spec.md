## MODIFIED Requirements

### Requirement: SDL3 Window Creation
The application SHALL create an SDL3 window with Vulkan support enabled on startup **unless `--headless` is active**, in which case SDL SHALL NOT be initialized and no window SHALL be created.

#### Scenario: Window creation success
- **GIVEN** SDL3 is properly initialized
- **WHEN** the application starts without `--headless`
- **THEN** a window titled "Goggles" SHALL be created
- **AND** the window SHALL have the Vulkan flag set

#### Scenario: SDL3 initialization failure
- **GIVEN** SDL3 cannot be initialized
- **WHEN** the application starts without `--headless`
- **THEN** an error SHALL be logged
- **AND** the application SHALL exit with a non-zero code

#### Scenario: Headless mode skips SDL entirely
- **GIVEN** the application is launched with `--headless`
- **WHEN** initialization runs
- **THEN** `SDL_Init` SHALL NOT be called
- **AND** no SDL window SHALL be created

## ADDED Requirements

### Requirement: Headless Mode CLI Flags

The application SHALL accept `--headless`, `--frames <N>`, and `--output <path>` as top-level CLI flags. `--frames` and `--output` are required when `--headless` is present; providing either without the other SHALL produce an error.

#### Scenario: Valid headless invocation
- **WHEN** run with `--headless --frames 10 --output /tmp/frame.png -- ./app`
- **THEN** `CliOptions.headless` SHALL be `true`, `frames` SHALL be `10`, `output_path` SHALL be `/tmp/frame.png`

#### Scenario: --frames without --output
- **WHEN** run with `--headless --frames 10 -- ./app` and `--output` is absent
- **THEN** the application SHALL print a descriptive error and exit with a non-zero code
