## ADDED Requirements

### Requirement: Filter Chain Control Scope and Precedence
The application UI SHALL expose three filter-related controls with distinct scope and precedence:
- `Application -> Window Management -> Filter Chain (All Surfaces)` controls global prechain/effect enablement.
- `Application -> Window Management -> Surface List` controls per-surface prechain/effect enablement.
- `Shader Controls -> Effect Stage (RetroArch) -> Enable Shader` controls effect stage only.

The application SHALL resolve an effective runtime policy that applies precedence as:
1) global toggle,
2) per-surface toggle,
3) effect-stage toggle.

The application SHALL dispatch the resolved policy through a single runtime update path so prechain
and effect stage updates occur together.

For first-time surface discovery, the application SHALL use deterministic defaulting rules:
- In direct Vulkan capture sessions, newly discovered active Vulkan-target surfaces default to
  filter-chain enabled.
- Once the user toggles a surface, the user choice SHALL be preserved and SHALL NOT be overwritten
  by subsequent auto-default evaluation.

When a direct Vulkan capture session initializes prechain defaults and no explicit prechain target
is configured, the application SHALL initialize prechain target from viewer swapchain extent.

#### Scenario: Global toggle disables all surfaces
- **GIVEN** the Window Management panel is visible
- **WHEN** the user disables `Filter Chain (All Surfaces)`
- **THEN** subsequent frames SHALL bypass prechain and effect stages for all surfaces

#### Scenario: Per-surface toggle applies when global is enabled
- **GIVEN** `Filter Chain (All Surfaces)` is enabled
- **WHEN** the user disables a surface entry in Surface List
- **THEN** subsequent frames for that surface SHALL bypass prechain and effect stages
- **AND** other enabled surfaces SHALL continue using prechain/effect

#### Scenario: Effect toggle does not disable prechain
- **GIVEN** global and per-surface toggles are enabled
- **WHEN** the user disables `Enable Shader`
- **THEN** subsequent frames SHALL bypass effect stage only
- **AND** prechain behavior SHALL remain controlled by global/per-surface toggles

#### Scenario: Runtime updates are applied atomically
- **GIVEN** any toggle transition changes effective stage policy
- **WHEN** the application dispatches runtime state to the backend
- **THEN** prechain and effect stage updates SHALL be applied together in one policy update

#### Scenario: First discovery defaults ON for direct Vulkan sessions
- **GIVEN** a direct Vulkan capture session and a newly discovered active surface
- **WHEN** the surface appears in Surface List without user override
- **THEN** its per-surface filter toggle SHALL default to enabled

#### Scenario: User override remains authoritative
- **GIVEN** a surface has been manually toggled by the user
- **WHEN** the surface list is refreshed or source timing changes
- **THEN** the surface toggle SHALL keep the user-selected value
