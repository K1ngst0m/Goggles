## ADDED Requirements

### Requirement: Surface Selection Requests

The system SHALL allow requesting focus and presentation of a specific tracked surface without
disabling automatic surface selection.

The compositor server SHALL provide `set_input_target(uint32_t surface_id)` which:
- Focuses the requested surface on the compositor thread
- Refreshes presentation to the focused surface
- Does not suppress automatic selection of newly mapped surfaces

#### Scenario: User selects a surface to focus
- **WHEN** two surfaces are connected
- **AND** surface A currently has keyboard and pointer focus
- **AND** `set_input_target(surface_b_id)` is called
- **THEN** surface B receives keyboard and pointer focus
- **AND** presentation switches to surface B

#### Scenario: New surface still auto-focuses after a selection
- **GIVEN** surface B was focused via `set_input_target(surface_b_id)`
- **WHEN** a new surface C is mapped
- **THEN** surface C receives keyboard and pointer focus

## MODIFIED Requirements

### Requirement: Surface Tracking

The system SHALL track surfaces from both xdg_shell (Wayland native) and XWayland clients.

The compositor server SHALL:
- Listen for `new_toplevel` signals from xdg_shell
- Listen for `new_surface` signals from XWayland
- Maintain a unified list of active surfaces (Wayland surfaces only)
- Register destroy listeners for Wayland surfaces only
- Auto-focus the most recently mapped surface
- Use mutual exclusion to manage focus between Wayland and XWayland surfaces

**Important**: XWayland surfaces SHALL NOT use destroy listeners. XWayland destroy signals fire at
unpredictable times during normal X11 operation, causing input forwarding failures. Instead, stale
XWayland pointers are cleared during focus transitions.

#### Scenario: Wayland client connects and receives focus
- **WHEN** a Wayland client creates an xdg_toplevel
- **THEN** the surface is tracked by the compositor
- **AND** the new surface receives keyboard and pointer focus

#### Scenario: XWayland client connects and receives focus
- **WHEN** an X11 app creates a window via XWayland
- **THEN** the XWayland surface is tracked by the compositor (not in m_surfaces list)
- **AND** the new surface receives keyboard and pointer focus

#### Scenario: Wayland client disconnects
- **WHEN** a tracked Wayland surface is destroyed
- **THEN** the surface is removed from tracking via destroy listener
- **AND** if it was focused, focus is cleared

#### Scenario: XWayland client disconnects
- **WHEN** an X11 app exits
- **THEN** no destroy listener fires (by design)
- **AND** m_focused_xsurface becomes a dangling pointer
- **AND** when a new surface gains focus, stale XWayland pointers are cleared safely
