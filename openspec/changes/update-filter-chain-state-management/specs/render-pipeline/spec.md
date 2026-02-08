## MODIFIED Requirements

### Requirement: Per-Surface Filter Chain Routing
The render pipeline SHALL honor a per-surface filter-chain enable flag and a session-wide global
enable flag when deciding whether to execute prechain and effect processing for a frame.

The effect stage SHALL also respect `Shader Controls -> Effect Stage (RetroArch) -> Enable Shader`.

When the global flag is disabled, the pipeline SHALL bypass prechain and effect stages for all
surfaces and render captured surfaces using a compositor-style maximize resize so the client
re-renders at the window size (no stretch-blit).

When the global flag is enabled but the per-surface flag is disabled, the pipeline SHALL bypass
prechain and effect stages for that surface and render it using a compositor-style maximize resize
so the client re-renders at the window size (no stretch-blit).

The postchain output blit SHALL remain active for presentation in all modes, including global
bypass mode.

The per-surface mode SHALL apply to the entire xdg_toplevel surface, including all popups and
subsurfaces belonging to that toplevel.

The runtime SHALL resolve the effective stage policy once per frame and apply it atomically so
prechain/effect stage state cannot diverge during toggle transitions.

The runtime SHALL preserve the active policy across filter-chain recreation and async chain swap so
new chain instances start with the same effective stage policy before rendering their first frame.

The runtime SHALL avoid startup-order-dependent behavior between source capture arrival, compositor
resize requests, and prechain target initialization.

In direct Vulkan capture sessions, the default prechain target initialization SHALL use viewer
swapchain extent unless the user/config explicitly sets a prechain resolution.

Compositor maximize/restore requests tied to filter policy SHALL be emitted on effective policy
transitions (or surface topology changes), not as unconditional periodic requests.

#### Scenario: Default uses filter chain
- **GIVEN** a surface has no explicit override
- **WHEN** a frame is rendered for that surface
- **THEN** prechain and effect stages SHALL execute for that frame

#### Scenario: Bypass filter chain for a surface
- **GIVEN** a surface has filter-chain disabled
- **WHEN** a frame is rendered for that surface
- **THEN** prechain and effect stages SHALL be bypassed
- **AND** the surface SHALL be rendered via a maximize-style resize without stretch-blit
- **AND** the postchain output blit SHALL still present the frame

#### Scenario: Global bypass overrides per-surface
- **GIVEN** the global filter-chain flag is disabled
- **WHEN** a frame is rendered for any surface
- **THEN** prechain and effect stages SHALL be bypassed
- **AND** the surface SHALL be rendered via a maximize-style resize without stretch-blit
- **AND** the postchain output blit SHALL still present the frame

#### Scenario: Effect stage toggle only affects effect stage
- **GIVEN** global and per-surface filter-chain flags are enabled
- **AND** `Enable Shader` is disabled
- **WHEN** a frame is rendered
- **THEN** prechain SHALL execute
- **AND** effect stage SHALL be bypassed
- **AND** postchain output blit SHALL present the frame

#### Scenario: Popup inherits parent mode
- **GIVEN** an xdg_toplevel surface has filter-chain disabled
- **WHEN** a popup or subsurface belonging to that toplevel is rendered
- **THEN** the popup SHALL be rendered with prechain/effect bypass and maximize-style resize without stretch-blit

#### Scenario: Async chain swap keeps active policy
- **GIVEN** the runtime has an active effective stage policy
- **WHEN** an async shader reload completes and swaps in a new chain instance
- **THEN** the new chain SHALL use the active effective stage policy on its first rendered frame
- **AND** no frame SHALL be rendered with default stage policy values

#### Scenario: Direct Vulkan startup is deterministic
- **GIVEN** the session uses direct Vulkan capture
- **WHEN** the application starts and filter chain is enabled
- **THEN** prechain default target initialization SHALL use viewer swapchain extent
- **AND** startup SHALL not depend on first-arrival source-frame extent

#### Scenario: Resize requests do not oscillate during startup
- **GIVEN** startup state is settling and surfaces are enumerating
- **WHEN** effective filter policy for a surface has not changed
- **THEN** the runtime SHALL NOT repeatedly emit contradictory maximize/restore resize requests
