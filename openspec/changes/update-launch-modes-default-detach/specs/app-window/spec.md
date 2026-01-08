## MODIFIED Requirements

### Requirement: Command Line Interface
The application SHALL support command-line arguments to provide a simplified “full feature” launch UX while keeping invalid option combinations unrepresentable.

#### Scenario: Default mode launch (full feature)
- **GIVEN** the user launches Goggles in default mode (no `--detach`)
- **WHEN** a target application command is provided after `--`
- **THEN** Goggles SHALL enable input forwarding
- **AND** it SHALL launch the target application with `GOGGLES_CAPTURE=1`
- **AND** it SHALL launch the target application with `GOGGLES_WSI_PROXY=1`
- **AND** it SHALL set both `DISPLAY` and `WAYLAND_DISPLAY` for the target application

#### Scenario: Default mode requires target app command
- **GIVEN** the user launches Goggles in default mode (no `--detach`)
- **WHEN** no target application command is provided
- **THEN** argument parsing SHALL fail with a non-zero exit code

#### Scenario: Detach mode disables input forwarding
- **GIVEN** the user launches Goggles with `--detach`
- **WHEN** Goggles starts
- **THEN** input forwarding SHALL be disabled

#### Scenario: Width/height flags are default-mode only
- **GIVEN** the user launches Goggles with `--detach`
- **WHEN** `--app-width` or `--app-height` is provided
- **THEN** argument parsing SHALL fail with a non-zero exit code

## ADDED Requirements

### Requirement: Default-Mode App Size Configuration
The application SHALL allow configuring the WSI proxy virtual surface size via CLI options when running in default mode.

#### Scenario: Apply custom virtual surface size in default mode
- **GIVEN** Goggles is running in default mode (no `--detach`)
- **AND** `--app-width` and `--app-height` are provided as positive integers
- **WHEN** Goggles launches the target application
- **THEN** it SHALL set `GOGGLES_WIDTH` and `GOGGLES_HEIGHT` to the provided values for that process
