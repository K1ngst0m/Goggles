## ADDED Requirements

### Requirement: Render Scale Mode Ownership

The render backend SHALL own the active scale mode and integer scale values and expose them for application and UI synchronization.

#### Scenario: Query returns current runtime state

- **GIVEN** the active scale mode has been updated at runtime
- **WHEN** the application queries the render backend for the active scale mode
- **THEN** the backend SHALL return the updated mode
- **AND** the current integer scale SHALL be available alongside it

### Requirement: Runtime Scale Mode Switching

The viewer SHALL allow switching the render scale mode at runtime and apply changes to subsequent frames without restart.

#### Scenario: UI change updates active scale mode

- **GIVEN** the shader controls window is visible
- **WHEN** the user selects a new scale mode in the Pre-Chain section
- **THEN** the active render scale mode SHALL update without restart
- **AND** subsequent frames SHALL use the new mode

#### Scenario: Dynamic mode request uses active backend state

- **GIVEN** the capture receiver is connected
- **WHEN** the active scale mode is `dynamic` and the swapchain extent changes or dynamic mode becomes active
- **THEN** the viewer SHALL request the source resolution to match the swapchain extent
- **AND** no request SHALL be sent when the active scale mode is not `dynamic`

### Requirement: Pre-Chain Scale Mode Controls

The Pre-Chain stage UI SHALL expose controls for the viewer scale mode.

#### Scenario: Scale mode selector is available

- **GIVEN** the shader controls window is visible
- **WHEN** the Pre-Chain section is expanded
- **THEN** a scale mode selector SHALL be displayed
- **AND** it SHALL include fit, fill, stretch, integer, and dynamic options

#### Scenario: Integer scale input visibility

- **GIVEN** the scale mode selector is set to `integer`
- **WHEN** the Pre-Chain section is visible
- **THEN** an integer scale input SHALL be displayed
- **AND** changes SHALL update the active integer scale at runtime
