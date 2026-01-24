## ADDED Requirements

### Requirement: Shader Stage UI Organization

The shader controls window SHALL organize controls into three collapsible sections corresponding to pipeline stages: Pre-Chain, Effect, and Post-Chain.

#### Scenario: Pre-chain section displays downsample controls

- **GIVEN** the shader controls window is visible
- **WHEN** the Pre-Chain section is expanded
- **THEN** resolution width and height input fields SHALL be displayed
- **AND** an "Apply" button SHALL be displayed to confirm changes

#### Scenario: Effect section displays RetroArch controls

- **GIVEN** the shader controls window is visible
- **WHEN** the Effect section is expanded
- **THEN** the shader enable checkbox SHALL be displayed
- **AND** the current preset label SHALL be displayed
- **AND** the available presets tree SHALL be displayed
- **AND** shader parameters SHALL be displayed if a preset is loaded

#### Scenario: Post-chain section displays placeholder

- **GIVEN** the shader controls window is visible
- **WHEN** the Post-Chain section is expanded
- **THEN** a label indicating "Output Blit" SHALL be displayed
- **AND** no controls SHALL be displayed

### Requirement: Pre-Chain Pipeline Configuration

The filter chain SHALL support runtime updates to pre-chain pipeline configuration (resolution) without requiring application restart. Pipeline configuration is distinct from shader parameters - it affects resource allocation and triggers pass rebuilds.

#### Scenario: Resolution update triggers pass rebuild

- **GIVEN** a pre-chain downsample pass exists
- **WHEN** `FilterChain::set_prechain_resolution(width, height)` is called with new values
- **THEN** existing pre-chain passes and framebuffers SHALL be cleared
- **AND** new passes SHALL be created on the next frame with the updated resolution

#### Scenario: Resolution query returns current state

- **GIVEN** a pre-chain resolution is configured
- **WHEN** `FilterChain::get_prechain_resolution()` is called
- **THEN** the current target width and height SHALL be returned

#### Scenario: Zero resolution disables pre-chain

- **GIVEN** pre-chain passes exist
- **WHEN** `set_prechain_resolution(0, 0)` is called
- **THEN** pre-chain processing SHALL be disabled
- **AND** captured frames SHALL pass directly to effect stage

### Requirement: Pre-Chain UI State Synchronization

The UI layer SHALL maintain synchronized state with the filter chain pre-chain configuration.

#### Scenario: UI initialized from backend state

- **GIVEN** the application starts with `--app-width 640 --app-height 480`
- **WHEN** the ImGui layer is initialized
- **THEN** the pre-chain resolution inputs SHALL display 640 and 480

#### Scenario: UI callback propagates changes

- **GIVEN** the pre-chain section is visible
- **WHEN** the user changes resolution and clicks Apply
- **THEN** the pre-chain change callback SHALL be invoked
- **AND** the new resolution SHALL be passed to the filter chain
