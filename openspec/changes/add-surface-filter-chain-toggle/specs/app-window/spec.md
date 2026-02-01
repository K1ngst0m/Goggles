## ADDED Requirements

### Requirement: Surface List Filter Chain Toggle
The Goggles application SHALL provide a per-surface checkbox in
`Application -> Window Management -> Surface List` that controls whether the filter chain is
applied to that surface.

Each surface entry SHALL display a checkbox with a short icon label (e.g., `FX`) and a tooltip
describing the filter-chain toggle. The checkbox SHALL be checked when filter chain processing is
enabled for that surface. Unchecking the box SHALL request a bypass mode that uses a real
maximize-style resize so the client re-renders at the window size (no stretch-blit).

Newly discovered surfaces SHALL default to filter chain enabled when the surface uses the Vulkan capture path, and disabled when the surface uses a non-Vulkan capture path.

#### Scenario: Surface list shows per-surface toggle
- **GIVEN** the Surface List window is visible
- **WHEN** surfaces are listed
- **THEN** each surface entry SHALL display a filter-chain checkbox reflecting the current per-surface state

#### Scenario: User disables filter chain for a surface
- **GIVEN** the Surface List window is visible
- **WHEN** the user unchecks a surface's filter-chain checkbox
- **THEN** the UI SHALL emit an update request for that surface's render mode
- **AND** the UI SHALL indicate the surface is in bypass mode

#### Scenario: Toggle shows tooltip
- **GIVEN** the Surface List window is visible
- **WHEN** the user hovers the filter-chain checkbox for a surface
- **THEN** a tooltip SHALL describe that the toggle enables or bypasses the filter chain for that surface

#### Scenario: Vulkan surface defaults to enabled
- **WHEN** a new surface using the Vulkan capture path is added to the Surface List
- **THEN** its filter-chain checkbox SHALL be checked by default

#### Scenario: Non-Vulkan surface defaults to disabled
- **WHEN** a new surface using a non-Vulkan capture path is added to the Surface List
- **THEN** its filter-chain checkbox SHALL be unchecked by default

### Requirement: Window Management Filter Chain Global Toggle
The Goggles application SHALL provide a global filter-chain checkbox in `Application -> Window Management` that enables or bypasses filter chain processing for all surfaces during the current session.
The effect stage remains additionally gated by `Shader Controls -> Effect Stage (RetroArch) -> Enable Shader`.

#### Scenario: Global toggle disables filter chain
- **GIVEN** the Window Management panel is visible
- **WHEN** the user unchecks the global filter-chain checkbox
- **THEN** the UI SHALL emit a session-wide request to bypass filter chain processing
- **AND** per-surface checkboxes SHALL remain visible but treated as disabled overrides while global bypass is active

#### Scenario: Global toggle restores per-surface behavior
- **GIVEN** the global filter-chain checkbox is unchecked
- **WHEN** the user re-checks the global filter-chain checkbox
- **THEN** filter chain processing SHALL resume using each surface's per-surface setting
