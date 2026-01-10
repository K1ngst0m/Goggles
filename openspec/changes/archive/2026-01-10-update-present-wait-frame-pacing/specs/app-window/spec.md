## ADDED Requirements

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
