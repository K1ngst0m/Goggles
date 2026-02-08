## ADDED Requirements

### Requirement: wlroots 0.19 Vulkan Renderer Baseline
The dependency stack SHALL use wlroots 0.19.x for compositor integration and SHALL build it with Vulkan renderer support enabled.

#### Scenario: Pixi package baseline for wlroots
- **GIVEN** project dependencies are resolved through Pixi
- **WHEN** compositor-capable environments are built
- **THEN** wlroots package version SHALL be pinned to the 0.19.x line

#### Scenario: Vulkan renderer support enabled
- **GIVEN** wlroots is built for Goggles compositor use
- **WHEN** build configuration is evaluated
- **THEN** Vulkan renderer support SHALL be enabled in wlroots build options

#### Scenario: Runtime availability
- **GIVEN** the default Pixi environment is active
- **WHEN** Goggles compositor mode initializes renderer creation
- **THEN** required wlroots Vulkan renderer APIs SHALL be available at runtime
