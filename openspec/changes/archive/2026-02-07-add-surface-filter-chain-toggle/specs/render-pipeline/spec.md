## ADDED Requirements

### Requirement: Per-Surface Filter Chain Routing
The render pipeline SHALL honor a per-surface filter-chain enable flag and a session-wide global
enable flag when deciding whether to execute the filter chain for a frame.

The effect stage SHALL also respect `Shader Controls -> Effect Stage (RetroArch) -> Enable Shader`.

When the global flag is disabled, the pipeline SHALL bypass the filter chain for all surfaces and
render captured surfaces using a compositor-style maximize resize so the client re-renders at the
window size (no stretch-blit).

When the global flag is enabled but the per-surface flag is disabled, the pipeline SHALL bypass the
filter chain for that surface and render it using a compositor-style maximize resize so the client
re-renders at the window size (no stretch-blit).

The per-surface mode SHALL apply to the entire xdg_toplevel surface, including all popups and subsurfaces belonging to that toplevel.

#### Scenario: Default uses filter chain
- **GIVEN** a surface has no explicit override
- **WHEN** a frame is rendered for that surface
- **THEN** the filter chain SHALL be executed for that frame

#### Scenario: Bypass filter chain for a surface
- **GIVEN** a surface has filter-chain disabled
- **WHEN** a frame is rendered for that surface
- **THEN** the filter chain SHALL be bypassed
- **AND** the surface SHALL be rendered via a maximize-style resize without stretch-blit

#### Scenario: Global bypass overrides per-surface
- **GIVEN** the global filter-chain flag is disabled
- **WHEN** a frame is rendered for any surface
- **THEN** the filter chain SHALL be bypassed
- **AND** the surface SHALL be rendered via a maximize-style resize without stretch-blit

#### Scenario: Popup inherits parent mode
- **GIVEN** an xdg_toplevel surface has filter-chain disabled
- **WHEN** a popup or subsurface belonging to that toplevel is rendered
- **THEN** the popup SHALL be rendered with filter-chain bypass and maximize-style resize without stretch-blit
