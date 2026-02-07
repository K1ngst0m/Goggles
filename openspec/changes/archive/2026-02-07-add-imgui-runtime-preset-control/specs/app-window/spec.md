## ADDED Requirements
### Requirement: Docked ImGui Control Surface
The Goggles application SHALL embed the latest Dear ImGui docking branch UI inside the SDL3 window to expose runtime controls without impacting the Vulkan capture layer.

#### Scenario: ImGui docking build
- **GIVEN** the Goggles app target is built
- **WHEN** dependencies are resolved
- **THEN** the Dear ImGui docking branch (>= 1.91) SHALL be linked into the app executable
- **AND** an ImGui dockspace SHALL be rendered each frame after the filter chain output
- **AND** SDL3 events SHALL be forwarded into ImGui so widgets respond to input

#### Scenario: UI scope limited to app
- **GIVEN** the Vulkan capture layer is compiled separately
- **WHEN** ImGui is added to the repository
- **THEN** no ImGui headers or libraries SHALL be included or linked into `src/capture/vk_layer`
- **AND** only the Goggles app binary SHALL depend on ImGui symbols

### Requirement: Shader Control Panel UI
The Goggles application SHALL expose a dockable "Shader Controls" panel that shows the active shader preset, enumerates available presets, and emits runtime preset or passthrough selection events.

#### Scenario: Preset selection list
- **GIVEN** `config/goggles.toml` contains a `[shader] preset` value and the `shaders/` tree contains `.slangp` files
- **WHEN** the Shader Controls panel is opened
- **THEN** it SHALL display the active preset path
- **AND** it SHALL list discoverable preset entries sourced from the config value and presets found under `shaders/`
- **AND** selecting a preset from the list SHALL send a reload request containing the new path to the render pipeline

#### Scenario: Passthrough toggle from UI
- **GIVEN** the Shader Controls panel is visible
- **WHEN** the user toggles "Passthrough (no filter chain)"
- **THEN** the UI SHALL highlight the passthrough state
- **AND** it SHALL emit a selection event instructing the render pipeline to bypass or restore the filter chain accordingly

### Requirement: Shader Parameter Controls
The Goggles application SHALL expose runtime shader parameter editing through the ImGui interface, allowing users to adjust filter chain parameters in real-time.

#### Scenario: Parameter display
- **GIVEN** a shader preset is loaded with parameters (min, max, step, description)
- **WHEN** the Shader Controls panel displays parameters
- **THEN** it SHALL list all available parameters with their current values
- **AND** each parameter SHALL be editable via slider or input field respecting min/max bounds

#### Scenario: Parameter modification
- **GIVEN** the user adjusts a shader parameter value
- **WHEN** the new value is within valid bounds
- **THEN** the filter chain SHALL update the parameter override
- **AND** the change SHALL take effect on the next rendered frame without requiring preset reload

#### Scenario: Parameter reset
- **GIVEN** parameters have been modified from their defaults
- **WHEN** the user clicks "Reset to Defaults"
- **THEN** all parameter overrides SHALL be cleared
- **AND** the shader SHALL render using original preset values
