## ADDED Requirements

### Requirement: Surface Enumeration

The system SHALL provide an API to enumerate all connected surfaces with metadata.

The `SurfaceInfo` struct SHALL contain:
- `id`: Unique surface identifier (assigned on creation)
- `title`: Window title (from `WM_NAME` for XWayland, empty for XDG)
- `class_name`: Window class (from `WM_CLASS` for XWayland, empty for XDG)
- `width`, `height`: Surface dimensions in pixels
- `is_xwayland`: True for X11 surfaces, false for native Wayland
- `is_input_target`: True if this surface currently receives input

The `CompositorServer::get_surfaces()` method SHALL return a snapshot of all tracked surfaces.

#### Scenario: Surface list includes XWayland client
- **WHEN** an X11 app connects via XWayland
- **AND** `get_surfaces()` is called
- **THEN** the returned list includes the surface with `is_xwayland = true`
- **AND** `title` contains the X11 `WM_NAME` property value
- **AND** `class_name` contains the X11 `WM_CLASS` property value

#### Scenario: Surface list includes Wayland client
- **WHEN** a native Wayland client creates an xdg_toplevel
- **AND** `get_surfaces()` is called
- **THEN** the returned list includes the surface with `is_xwayland = false`
- **AND** `title` and `class_name` are empty strings

#### Scenario: Surface removed from list on disconnect
- **WHEN** a client disconnects
- **AND** `get_surfaces()` is called
- **THEN** the returned list does not include the disconnected surface

### Requirement: Manual Input Target Selection

The system SHALL allow manual selection of which surface receives input.

The `CompositorServer` SHALL provide:
- `set_input_target(uint32_t surface_id)`: Route input to specified surface
- `clear_input_override()`: Revert to automatic selection (first surface)

When a manual target is set, input SHALL be routed to that surface regardless of connection order.

When the manual target surface disconnects, the system SHALL clear the override and revert to automatic selection.

#### Scenario: Manual selection routes input
- **WHEN** two surfaces are connected
- **AND** `set_input_target(surface_2_id)` is called
- **THEN** input events are delivered to surface 2
- **AND** surface 1 does not receive input

#### Scenario: Clear override reverts to auto
- **WHEN** a manual target is active
- **AND** `clear_input_override()` is called
- **THEN** input is routed to the first connected surface (auto behavior)

#### Scenario: Manual target disconnect clears override
- **WHEN** `set_input_target(surface_id)` is active
- **AND** the target surface disconnects
- **THEN** the override is cleared automatically
- **AND** input is routed to the first remaining surface

### Requirement: Surface Selector UI

The system SHALL provide an ImGui window for viewing and selecting input targets.

The surface selector window SHALL:
- Toggle visibility with F4 key
- Display a list of all connected surfaces
- Show surface ID, title (or fallback), and dimensions for each entry
- Highlight the current input target
- Indicate whether selection is "(auto)" or "(manual)"
- Provide a "Reset to Auto" button to clear manual override
- Allow clicking a surface to select it as input target

#### Scenario: F4 toggles surface selector
- **WHEN** user presses F4
- **THEN** the surface selector window visibility toggles
- **AND** when visible, the current surface list is displayed

#### Scenario: Click surface to select
- **WHEN** the surface selector is visible
- **AND** user clicks a surface entry
- **THEN** that surface becomes the input target
- **AND** the indicator changes to "(manual)"

#### Scenario: Reset to Auto button
- **WHEN** a manual target is active
- **AND** user clicks "Reset to Auto"
- **THEN** the override is cleared
- **AND** the indicator changes to "(auto)"

## MODIFIED Requirements

### Requirement: Surface Tracking

The system SHALL track surfaces from both xdg_shell (Wayland native) and XWayland clients.

The compositor server SHALL:
- Listen for `new_toplevel` signals from xdg_shell
- Listen for `new_surface` signals from XWayland
- Maintain a unified list of active surfaces (Wayland surfaces only)
- Register destroy listeners for Wayland surfaces only
- Assign a unique ID to each surface on creation
- Query X11 window properties (`WM_NAME`, `WM_CLASS`) for XWayland surfaces
- Use manual target if set, otherwise auto-focus the first connected surface
- Use mutual exclusion to manage focus between Wayland and XWayland surfaces

**Important**: XWayland surfaces SHALL NOT use destroy listeners. XWayland destroy signals fire at unpredictable times during normal X11 operation, causing input forwarding failures. Instead, stale XWayland pointers are cleared during focus transitions.

#### Scenario: Wayland client connects and receives focus
- **WHEN** a Wayland client creates an xdg_toplevel
- **THEN** the surface is tracked by the compositor with a unique ID
- **AND** if no manual target is set and no surface was previously focused, the new surface receives keyboard and pointer focus
- **AND** if an XWayland surface had focus, the Wayland surface steals focus (XWayland pointer may be stale)

#### Scenario: XWayland client connects and receives focus
- **WHEN** an X11 app creates a window via XWayland
- **THEN** the XWayland surface is tracked by the compositor with a unique ID (not in m_surfaces list)
- **AND** X11 properties (`WM_NAME`, `WM_CLASS`) are queried and stored
- **AND** if no manual target is set and no surface was previously focused, the surface receives keyboard and pointer focus
- **AND** if a Wayland surface already has focus, the XWayland surface does NOT steal focus

#### Scenario: Wayland client disconnects
- **WHEN** a tracked Wayland surface is destroyed
- **THEN** the surface is removed from tracking via destroy listener
- **AND** if it was the manual target, the override is cleared
- **AND** if it was focused, focus is cleared

#### Scenario: XWayland client disconnects
- **WHEN** an X11 app exits
- **THEN** no destroy listener fires (by design)
- **AND** m_focused_xsurface becomes a dangling pointer
- **AND** if it was the manual target, the override is cleared on next focus attempt
- **AND** when a new surface gains focus, stale XWayland pointers are cleared safely
