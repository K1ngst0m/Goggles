# config-loading Specification

## Purpose
TBD - created by archiving change refactor-path-resolution-and-config-loading. Update Purpose after archive.
## Requirements
### Requirement: Config Discovery Uses XDG Config Home
The system SHALL discover the default configuration file at:
`${XDG_CONFIG_HOME:-$HOME/.config}/goggles/goggles.toml`.

#### Scenario: Default config path is resolved
- **GIVEN** no explicit `--config` argument is provided
- **WHEN** Goggles starts
- **THEN** it SHALL resolve the default config path under XDG config home

### Requirement: Config Is Validated Early With Fallback
The system SHALL validate the configuration file early during startup, before configuration-dependent
behavior is applied.

If the config file is missing, unreadable, fails to parse, or fails semantic validation, the system
SHALL:
- log a single warning at the application boundary
- fall back to defaults (and continue)

#### Scenario: Missing config falls back to defaults
- **GIVEN** no config file exists at the resolved default path
- **WHEN** Goggles starts
- **THEN** it SHALL log a warning
- **AND** it SHALL continue using default configuration values

#### Scenario: Invalid config falls back to defaults
- **GIVEN** a config file exists but contains invalid TOML or invalid values
- **WHEN** Goggles starts
- **THEN** it SHALL log a warning describing the failure
- **AND** it SHALL continue using default configuration values

#### Scenario: Explicit config path is invalid
- **GIVEN** the user provides an explicit `--config` path
- **AND** the config file is missing or invalid
- **WHEN** Goggles starts
- **THEN** it SHALL log a warning describing the failure
- **AND** it SHALL continue using default configuration values

### Requirement: Optional Template-Based Bootstrap
The system SHALL support an optional template-based bootstrap flow for creating a user config when
no user config exists.

If a shipped config template exists under `resource_dir`, the system MAY write a user config file into
XDG config home.

The write operation SHALL be atomic (no partial writes on crash/disk-full).
If writing fails, the system SHALL continue using defaults or the template with a warning.

#### Scenario: Template seeds a new user config
- **GIVEN** no user config exists
- **AND** a shipped template exists under `resource_dir`
- **WHEN** Goggles starts
- **THEN** it MAY write a new user config under XDG config home atomically
- **AND** it SHALL continue startup regardless of whether the write succeeds

### Requirement: Config Supports Directory Root Overrides
The system SHALL support an optional `[paths]` table in the configuration file to override directory
roots:
- `resource_dir`
- `config_dir`
- `data_dir`
- `cache_dir`
- `runtime_dir`

When provided, these overrides SHALL take precedence over environment variables and defaults.

#### Scenario: Config overrides cache directory
- **GIVEN** the config file sets `[paths].cache_dir`
- **WHEN** Goggles starts
- **THEN** it SHALL use the configured cache directory root instead of XDG-derived defaults

