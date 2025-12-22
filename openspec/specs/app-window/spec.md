# app-window Specification

## Purpose
TBD - created by archiving change add-sdl3-window-test. Update Purpose after archive.
## Requirements
### Requirement: SDL3 Window Creation
The application SHALL create an SDL3 window with Vulkan support enabled on startup.

#### Scenario: Window creation success
- **GIVEN** SDL3 is properly initialized
- **WHEN** the application starts
- **THEN** a window titled "Goggles" SHALL be created
- **AND** the window SHALL have the Vulkan flag set

#### Scenario: SDL3 initialization failure
- **GIVEN** SDL3 cannot be initialized
- **WHEN** the application starts
- **THEN** an error SHALL be logged
- **AND** the application SHALL exit with a non-zero code

### Requirement: Window Event Loop
The application SHALL run an event loop that processes window events until the user closes the window.

#### Scenario: Window close event
- **GIVEN** the window is open
- **WHEN** the user closes the window (X button or Alt+F4)
- **THEN** the event loop SHALL exit
- **AND** SDL3 resources SHALL be cleaned up

### Requirement: Command Line Interface
The application SHALL support command-line arguments to override default behavior and provide information without throwing exceptions into the main execution flow.

#### Scenario: Display help
- **WHEN** the application is run with `--help`
- **THEN** it SHALL print usage information
- **AND** it SHALL exit with code 0

#### Scenario: Display version
- **WHEN** the application is run with `--version`
- **THEN** it SHALL print "Goggles v0.1.0"
- **AND** it SHALL exit with code 0

#### Scenario: Exception encapsulation
- **GIVEN** the application uses CLI11 for parsing
- **WHEN** `parse_cli` is called
- **THEN** it MUST catch all library exceptions internally
- **AND** it MUST return a `nonstd::expected` value-based result

#### Scenario: Override shader preset
- **GIVEN** a valid `.slangp` file exists
- **WHEN** run with `--shader <path>`
- **THEN** the specified preset SHALL be loaded regardless of config file settings

