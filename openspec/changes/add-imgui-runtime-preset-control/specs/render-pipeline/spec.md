## ADDED Requirements
### Requirement: Runtime Shader Preset Reload
The render pipeline SHALL support rebuilding the RetroArch filter chain at runtime when the application requests a new `.slangp` preset without forcing an application restart.

#### Scenario: UI-triggered reload success
- **GIVEN** the Shader Controls panel emits a preset selection event containing `<path>`
- **WHEN** the render pipeline handles the event
- **THEN** it SHALL wait for the in-flight frame to complete
- **AND** it SHALL destroy the current filter chain state
- **AND** it SHALL parse and instantiate the new preset from `<path>`
- **AND** the next frame SHALL render using the newly loaded passes

#### Scenario: Reload failure fallback
- **GIVEN** the render pipeline attempts to load a preset that fails to parse or compile
- **WHEN** the failure occurs
- **THEN** the previously active preset SHALL remain bound and continue rendering
- **AND** the failure SHALL be reported back to the UI/log so the user can pick a different preset

### Requirement: Passthrough Mode Toggle
The render pipeline SHALL provide a passthrough mode that bypasses all filter passes and blits the captured frame directly when requested, while remembering the last successful preset for restoration.

#### Scenario: Passthrough enabled at runtime
- **GIVEN** the Shader Controls panel requests passthrough mode
- **WHEN** the render pipeline processes the request
- **THEN** it SHALL stop invoking the filter chain and route the captured texture directly into `OutputPass`
- **AND** no RetroArch preset compilation SHALL occur while passthrough is active

#### Scenario: Passthrough disabled restores preset
- **GIVEN** passthrough mode is active and the user turns it off
- **WHEN** the render pipeline receives the request
- **THEN** it SHALL reload the last successful preset (or the default from config if none exists)
- **AND** rendering SHALL resume using the restored filter chain without requiring an application restart
