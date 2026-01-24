## ADDED Requirements

### Requirement: Goggles Overlay Toggle

The viewer application SHALL use Ctrl+Alt+Shift+Q to toggle visibility of the Goggles Overlay.

When Goggles Overlay is hidden:
- All overlay windows SHALL be invisible
- All keyboard and mouse input SHALL be forwarded to the target application without interception
- The Ctrl+Alt+Shift+Q key combination itself SHALL NOT be forwarded (consumed by toggle)

When Goggles Overlay is visible:
- Shader Controls and Application windows appear side-by-side (dockable by user)
- Input forwarding to target application SHALL be blocked when ImGui wants keyboard/mouse capture

#### Scenario: User toggles overlay visibility
- **WHEN** user presses Ctrl+Alt+Shift+Q
- **AND** Goggles Overlay is currently hidden
- **THEN** the tabbed overlay windows become visible

#### Scenario: User hides overlay
- **WHEN** user presses Ctrl+Alt+Shift+Q
- **AND** Goggles Overlay is currently visible
- **THEN** all overlay windows are hidden

#### Scenario: All function keys forwarded to application
- **WHEN** user presses F1, F2, F3, or F4
- **THEN** the key event is forwarded to the target application
- **AND** no viewer UI action occurs

### Requirement: Dockable Windows

The Goggles Overlay windows SHALL be dockable via ImGui's docking feature.

- "Shader Controls" window for shader-related settings
- "Application" window for performance and input controls

Users MAY dock windows together as tabs if desired. The layout is saved in imgui.ini.

### Requirement: Application Window

The Application window SHALL consolidate all application and view controls.

The window SHALL include:
- A "Performance" collapsible section containing:
  - Render FPS histogram showing frame-to-frame timing
  - Source FPS histogram showing captured frame cadence
- An "Input" collapsible section containing:
  - A checkbox labeled "Force Enable Pointer Lock" to force pointer lock regardless of app requests
  - When pointer lock is enabled, a hint SHALL display: "Press Ctrl+Alt+Shift+Q to toggle overlay"
  - A list of connected surfaces for input target selection
  - A "Reset to Auto" button to clear manual surface selection

#### Scenario: User views performance metrics
- **WHEN** Application window is visible
- **AND** user expands the "Performance" section
- **THEN** FPS graphs and frame timing statistics are visible

#### Scenario: User enables forced pointer lock
- **WHEN** Application window is visible
- **AND** user checks "Force Enable Pointer Lock"
- **THEN** the viewer window enters relative mouse mode
- **AND** pointer lock is active regardless of target application requests
- **AND** a hint displays the overlay toggle shortcut

#### Scenario: User returns to automatic pointer lock
- **WHEN** Application window is visible
- **AND** user unchecks "Force Enable Pointer Lock"
- **THEN** pointer lock follows the target application's requests

### Requirement: Shader Controls Window

The Shader Controls window SHALL contain only shader-related settings.

The window SHALL include:
- Pre-Chain Stage controls (resolution profile, parameters)
- Effect Stage controls (preset selection, parameters)
- Post-Chain Stage controls (output parameters)

No view or application controls SHALL be present in the Shader Controls window.

#### Scenario: User adjusts shader settings
- **WHEN** Shader Controls window is visible
- **AND** user modifies a shader parameter
- **THEN** the shader pipeline reflects the change
- **AND** no application or view settings are affected

## REMOVED Requirements

### Requirement: Multiple Hotkey UI Controls
**Reason**: Consolidating to modifier-key toggle to reduce conflicts with target application hotkeys.
**Migration**: Users press Ctrl+Alt+Shift+Q to show Goggles Overlay.

The following keyboard shortcuts are removed:
- F1: Shader controls toggle
- F2: Debug overlay toggle
- F3: Pointer lock override toggle
- F4: Application management toggle
