# app-window Specification

## Purpose
TBD - created by archiving change add-sdl3-window-test. Update Purpose after archive.
## Requirements
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

### Requirement: Headless Mode CLI Flags

The application SHALL accept `--headless`, `--frames <N>`, and `--output <path>` as top-level CLI flags. `--frames` and `--output` are required when `--headless` is present; providing either without the other SHALL produce an error.

#### Scenario: Valid headless invocation
- **WHEN** run with `--headless --frames 10 --output /tmp/frame.png -- ./app`
- **THEN** `CliOptions.headless` SHALL be `true`, `frames` SHALL be `10`, `output_path` SHALL be `/tmp/frame.png`

#### Scenario: --frames without --output
- **WHEN** run with `--headless --frames 10 -- ./app` and `--output` is absent
- **THEN** the application SHALL print a descriptive error and exit with a non-zero code

### Requirement: Filter Chain Control Scope and Precedence
The application UI SHALL expose three filter-related controls with distinct scope and precedence:
- `Application -> Window Management -> Filter Chain (All Surfaces)` controls global prechain/effect enablement.
- `Application -> Window Management -> Surface List` controls per-surface prechain/effect enablement.
- `Shader Controls -> Effect Stage (RetroArch) -> Enable Shader` controls effect stage only.

The application SHALL resolve an effective runtime policy that applies precedence as:
1) global toggle,
2) per-surface toggle,
3) effect-stage toggle.

The application SHALL dispatch the resolved policy through a single runtime update path so prechain
and effect stage updates occur together.

For first-time surface discovery, the application SHALL use deterministic defaulting rules:
- In direct Vulkan capture sessions, newly discovered active Vulkan-target surfaces default to
  filter-chain enabled.
- Once the user toggles a surface, the user choice SHALL be preserved and SHALL NOT be overwritten
  by subsequent auto-default evaluation.

When a direct Vulkan capture session initializes prechain defaults and no explicit prechain target
is configured, the application SHALL initialize prechain target from viewer swapchain extent.

#### Scenario: Global toggle disables all surfaces
- **GIVEN** the Window Management panel is visible
- **WHEN** the user disables `Filter Chain (All Surfaces)`
- **THEN** subsequent frames SHALL bypass prechain and effect stages for all surfaces

#### Scenario: Per-surface toggle applies when global is enabled
- **GIVEN** `Filter Chain (All Surfaces)` is enabled
- **WHEN** the user disables a surface entry in Surface List
- **THEN** subsequent frames for that surface SHALL bypass prechain and effect stages
- **AND** other enabled surfaces SHALL continue using prechain/effect

#### Scenario: Effect toggle does not disable prechain
- **GIVEN** global and per-surface toggles are enabled
- **WHEN** the user disables `Enable Shader`
- **THEN** subsequent frames SHALL bypass effect stage only
- **AND** prechain behavior SHALL remain controlled by global/per-surface toggles

#### Scenario: Runtime updates are applied atomically
- **GIVEN** any toggle transition changes effective stage policy
- **WHEN** the application dispatches runtime state to the backend
- **THEN** prechain and effect stage updates SHALL be applied together in one policy update

#### Scenario: First discovery defaults ON for direct Vulkan sessions
- **GIVEN** a direct Vulkan capture session and a newly discovered active surface
- **WHEN** the surface appears in Surface List without user override
- **THEN** its per-surface filter toggle SHALL default to enabled

#### Scenario: User override remains authoritative
- **GIVEN** a surface has been manually toggled by the user
- **WHEN** the surface list is refreshed or source timing changes
- **THEN** the surface toggle SHALL keep the user-selected value

### Requirement: Application Performance Panel Reports Gamer-Facing Metrics

The Application performance panel SHALL report `Game FPS` and `Compositor Latency` instead of the
legacy `Render` and `Source` FPS metrics.

The panel SHALL display compositor-provided `Game FPS` and `Compositor Latency` values.

#### Scenario: Performance panel shows replacement metrics
- **WHEN** the Application performance panel is rendered
- **THEN** it SHALL display `Game FPS` and `Compositor Latency`
- **AND** it SHALL NOT display `Render` FPS or `Source` FPS

#### Scenario: Legacy performance plots are removed
- **WHEN** the Application performance panel is rendered after this change
- **THEN** it SHALL NOT render the legacy frame-history plots associated with `Render` and `Source`
  FPS

#### Scenario: Game FPS follows active captured game surface only
- **GIVEN** a game surface is the current capture target
- **WHEN** the performance panel reports `Game FPS`
- **THEN** the reported value SHALL come from the compositor-provided metric snapshot for that
  capture target only
