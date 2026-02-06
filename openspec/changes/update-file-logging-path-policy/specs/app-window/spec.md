# app-window Specification (Delta)

## ADDED Requirements

### Requirement: App Logger Supports Optional File Sink
The application SHALL support optional file logging driven by `[logging].file` in the loaded config.

If `logging.file` is empty, the app SHALL keep console-only logging.
If `logging.file` is non-empty, the app SHALL configure a file sink in addition to console logging.

#### Scenario: Empty logging.file keeps console-only behavior
- **GIVEN** `[logging].file = ""`
- **WHEN** the application starts and applies logging configuration
- **THEN** no file sink SHALL be attached
- **AND** console logging SHALL remain active

#### Scenario: Non-empty logging.file enables file logging
- **GIVEN** `[logging].file` is configured with a valid file path
- **WHEN** the application starts and applies logging configuration
- **THEN** a file sink SHALL be attached to the app logger
- **AND** log records SHALL be written to both console and file sinks

#### Scenario: File sink setup failure is non-fatal
- **GIVEN** `[logging].file` is configured but cannot be opened or created
- **WHEN** the application starts and applies logging configuration
- **THEN** the application SHALL log one warning at the app boundary
- **AND** the application SHALL continue startup using console logging only

### Requirement: Relative logging.file Paths Are Resolved Against Config Origin
The application SHALL resolve relative `[logging].file` paths against the directory containing the
config file that provided the `logging.file` value.

#### Scenario: Relative path from default config location
- **GIVEN** config is loaded from `${XDG_CONFIG_HOME:-$HOME/.config}/goggles/goggles.toml`
- **AND** `[logging].file = "logs/goggles.log"`
- **WHEN** logging is configured
- **THEN** the effective log path SHALL be `${XDG_CONFIG_HOME:-$HOME/.config}/goggles/logs/goggles.log`

#### Scenario: Relative path from explicit --config location
- **GIVEN** config is loaded from an explicit `--config /custom/path/goggles.toml`
- **AND** `[logging].file = "logs/goggles.log"`
- **WHEN** logging is configured
- **THEN** the effective log path SHALL be `/custom/path/logs/goggles.log`

#### Scenario: Relative path is independent of current working directory
- **GIVEN** the same config file path and `logging.file` value
- **WHEN** the application is started from different working directories
- **THEN** the effective log file path SHALL remain identical
