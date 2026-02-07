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

### Requirement: SDL Resource Ownership via RAII

The application SHALL manage SDL initialization and the SDL window lifetime via RAII wrappers within the app module to ensure SDL resources are cleaned up on all exit paths, including early returns due to initialization failures.

#### Scenario: Window creation failure cleanup
- **GIVEN** SDL3 initializes successfully
- **WHEN** window creation fails
- **THEN** an error SHALL be logged
- **AND** SDL3 resources SHALL be cleaned up before exit

### Requirement: Orchestrated Event Loop Boundary

The application SHALL encapsulate window event handling and per-frame orchestration behind a dedicated component (e.g., `goggles::app::Application`), keeping `src/app/main.cpp` limited to composition and top-level error handling.

#### Scenario: Quit event exits orchestration
- **GIVEN** the window is open
- **WHEN** the user closes the window (X button or Alt+F4)
- **THEN** the orchestrator SHALL stop the event loop
- **AND** SDL3 resources SHALL be cleaned up

### Requirement: Child Process Death Signal

The application SHALL configure spawned child processes to receive SIGTERM when the parent process terminates unexpectedly.

#### Scenario: Parent crash terminates child

- **GIVEN** a child process was spawned via `-- <app>` mode
- **WHEN** the parent goggles process is killed (SIGKILL, crash, or abnormal termination)
- **THEN** the child process SHALL receive SIGTERM automatically
- **AND** the child process SHALL terminate

#### Scenario: Parent PID 1 reparenting race

- **GIVEN** a child process is being spawned
- **WHEN** the parent dies between `fork()` and `prctl()` setup
- **THEN** the child SHALL detect reparenting to PID 1
- **AND** SHALL exit immediately with failure code

### Requirement: Target FPS CLI Override

The application SHALL allow overriding the render target FPS from the command line.

#### Scenario: Override target fps via CLI
- **GIVEN** the application is started with `--target-fps 120`
- **WHEN** configuration is loaded
- **THEN** `config.render.target_fps` SHALL be set to `120`
- **AND** the override SHALL take precedence over the config file

#### Scenario: Disable frame cap via CLI
- **GIVEN** the application is started with `--target-fps 0`
- **WHEN** configuration is loaded
- **THEN** `config.render.target_fps` SHALL be set to `0`
- **AND** presentation pacing SHALL be uncapped

### Requirement: GPU Device Selection

The application SHALL allow users to select a specific GPU and SHALL improve automatic GPU
selection for multi-GPU systems.

#### Scenario: Explicit GPU selection by index

- **GIVEN** multiple GPUs are available
- **WHEN** the user runs with `--gpu 0`
- **THEN** the application SHALL use the GPU at index 0

#### Scenario: Explicit GPU selection by name

- **GIVEN** multiple GPUs are available including one with "AMD" in its name
- **WHEN** the user runs with `--gpu AMD`
- **THEN** the application SHALL use the GPU whose name contains "AMD"

#### Scenario: Invalid GPU selector

- **GIVEN** no GPU matches the selector
- **WHEN** the user runs with `--gpu nonexistent`
- **THEN** the application SHALL exit with an error message listing available GPUs

#### Scenario: Ambiguous GPU selector

- **GIVEN** multiple suitable GPUs match the name selector
- **WHEN** the user runs with a non-unique selector like `--gpu AMD`
- **THEN** the application SHALL exit with an error listing matching GPUs
- **AND** it SHALL instruct the user to choose a numeric index

#### Scenario: Default GPU selection

- **GIVEN** multiple GPUs are available and no `--gpu` option is specified
- **WHEN** the application selects a GPU
- **THEN** it SHALL prefer a GPU that supports presenting to the current surface
- **AND** it SHALL log all available GPUs with their indices
