# app-window Spec Delta

## ADDED Requirements

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

