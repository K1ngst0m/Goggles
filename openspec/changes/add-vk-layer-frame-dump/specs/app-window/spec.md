## ADDED Requirements

### Requirement: Frame Dump CLI Env Overrides (Default Mode)

When running in default mode (launching a target app), the application SHALL provide CLI flags to
configure the capture layer frame dump feature for the launched target process.

#### Scenario: Dump CLI flags set target environment variables

- **GIVEN** the application is launched in default mode with `-- <app> [args...]`
- **WHEN** the user provides `--dump-dir <dir>`
- **THEN** the launched target process SHALL receive `GOGGLES_DUMP_DIR=<dir>`
- **WHEN** the user provides `--dump-frame-range <range>`
- **THEN** the launched target process SHALL receive `GOGGLES_DUMP_FRAME_RANGE=<range>`
- **WHEN** the user provides `--dump-frame-mode <mode>`
- **THEN** the launched target process SHALL receive `GOGGLES_DUMP_FRAME_MODE=<mode>`

#### Scenario: Dump CLI flags rejected in detach mode

- **GIVEN** the application is launched with `--detach`
- **WHEN** the user provides any of `--dump-dir`, `--dump-frame-range`, or `--dump-frame-mode`
- **THEN** CLI parsing SHALL fail with a parse error

