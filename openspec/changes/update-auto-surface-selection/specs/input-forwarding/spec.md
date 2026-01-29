## MODIFIED Requirements
### Requirement: Surface Tracking

The system SHALL track surfaces from both xdg_shell (Wayland native) and XWayland clients.

The compositor server SHALL:
- Listen for `new_toplevel` signals from xdg_shell
- Listen for `new_surface` signals from XWayland
- Maintain a unified list of active surfaces (Wayland surfaces only)
- Register destroy listeners for Wayland surfaces only
- Auto-focus the most recently mapped surface when automatic selection is active
- Preserve the manually selected surface when manual override is active
- Use mutual exclusion to manage focus between Wayland and XWayland surfaces

**Important**: XWayland surfaces SHALL NOT use destroy listeners. XWayland destroy signals fire at
unpredictable times during normal X11 operation, causing input forwarding failures. Instead, stale
XWayland pointers are cleared during focus transitions.

#### Scenario: Wayland client connects and receives focus
- **WHEN** a Wayland client creates an xdg_toplevel
- **AND** automatic selection is active
- **THEN** the surface is tracked by the compositor
- **AND** the new surface receives keyboard and pointer focus

#### Scenario: XWayland client connects and receives focus
- **WHEN** an X11 app creates a window via XWayland
- **AND** automatic selection is active
- **THEN** the XWayland surface is tracked by the compositor (not in m_surfaces list)
- **AND** the new surface receives keyboard and pointer focus

#### Scenario: New surface does not steal focus during manual override
- **GIVEN** manual surface selection is active
- **AND** surface A has keyboard and pointer focus
- **WHEN** a new surface is created
- **THEN** surface A retains keyboard and pointer focus

#### Scenario: Wayland client disconnects
- **WHEN** a tracked Wayland surface is destroyed
- **THEN** the surface is removed from tracking via destroy listener
- **AND** if it was focused, focus is cleared

#### Scenario: XWayland client disconnects
- **WHEN** an X11 app exits
- **THEN** no destroy listener fires (by design)
- **AND** m_focused_xsurface becomes a dangling pointer
- **AND** when a new surface gains focus, stale XWayland pointers are cleared safely
