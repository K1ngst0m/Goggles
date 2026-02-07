# input-forwarding Specification

## Purpose
TBD - created by archiving change add-wayland-input-forwarding. Update Purpose after archive.
## Requirements
### Requirement: Input Forwarding Infrastructure

The system SHALL provide a compositor server that supports both XWayland (for X11 apps) and native Wayland clients using a unified `wlr_seat` input path.

The compositor server SHALL:
- Create a headless wlroots backend
- Bind a Wayland socket for client connections
- Start XWayland server for X11 application support
- Create a wlr_seat with keyboard and pointer capabilities
- Connect XWayland to the seat via `wlr_xwayland_set_seat()`
- Run the compositor event loop on a dedicated thread

#### Scenario: Compositor initializes with unified input
- **WHEN** the input forwarding system starts
- **THEN** a Wayland socket is created (wayland-N)
- **AND** an XWayland server is started (DISPLAY :N)
- **AND** XWayland is connected to the seat for automatic input translation
- **AND** a seat with keyboard and pointer capabilities is available

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

### Requirement: Unified Keyboard Input

The system SHALL forward keyboard events to the focused surface using `wlr_seat_keyboard_*` APIs.

The implementation SHALL:
- Use `wlr_seat_keyboard_enter()` on surface focus
- Use `wlr_seat_keyboard_notify_key()` for key events
- Use `wlr_seat_keyboard_notify_modifiers()` for modifier state
- Marshal events from main thread to compositor thread via eventfd

The wlr_xwm SHALL automatically translate keyboard events to X11 for XWayland surfaces.

#### Scenario: Key event forwarded to Wayland client
- **WHEN** user presses a key in the viewer window
- **AND** a Wayland surface has keyboard focus
- **THEN** the key event is delivered via wl_keyboard.key protocol

#### Scenario: Key event forwarded to X11 client
- **WHEN** user presses a key in the viewer window
- **AND** an XWayland surface has keyboard focus
- **THEN** wlr_xwm translates the event to X11 KeyPress/KeyRelease
- **AND** the X11 app receives the event

### Requirement: Unified Pointer Input

The system SHALL forward pointer events (motion, button, axis) to the focused surface using
`wlr_seat_pointer_*` APIs.

The implementation SHALL:
- Use `wlr_seat_pointer_enter()` on surface focus with the compositor cursor position.
- Use `wlr_seat_pointer_notify_motion()` with compositor cursor coordinates for absolute motion.
- Use `wlr_seat_pointer_notify_button()` for all button events (extended set).
- Use `wlr_seat_pointer_notify_axis()` for scroll events.
- Use `wlr_seat_pointer_notify_frame()` to group related events.
- Send relative motion via `wlr_relative_pointer_manager_v1_send_relative_motion()` using raw
  deltas from the viewer (no scale adjustment).
- Respect active pointer constraints when processing motion.

The wlr_xwm SHALL automatically translate pointer events to X11 for XWayland surfaces.

#### Scenario: Absolute motion uses software cursor
- **WHEN** the user moves the mouse with no active pointer lock
- **THEN** `wl_pointer.motion` is sent using the compositor cursor position
- **AND** the rendered software cursor matches the motion on-screen

#### Scenario: Relative motion remains raw under scaling
- **WHEN** a client is bound to `zwp_relative_pointer_v1`
- **THEN** the client receives raw, unscaled deltas
- **AND** relative motion is delivered regardless of viewer scaling

### Requirement: Thread-Safe Event Marshaling

The system SHALL marshal input events from the main thread to the compositor thread safely.

The implementation SHALL:
- Use `SPSCQueue<InputEvent>` for lock-free event passing (per project threading policy)
- Use eventfd for wl_event_loop wakeup notification
- Process events on compositor thread via wl_event_loop integration
- Avoid blocking the main thread during event delivery

#### Scenario: Event delivered without blocking main thread
- **WHEN** an input event is forwarded
- **THEN** the main thread pushes to SPSCQueue and writes to eventfd
- **AND** the main thread returns immediately
- **AND** the compositor thread drains the queue and dispatches via wlr_seat_*

### Requirement: Coordinate Handling

The system SHALL derive compositor cursor motion from raw relative deltas and SHALL NOT rely on
viewer absolute coordinates.

The system SHALL:
- Maintain a compositor-local cursor position in surface coordinates for the focused surface.
- Apply raw relative motion deltas to the compositor cursor position.
- Clamp cursor motion to the surface bounds and any active pointer confinement region.
- Use the compositor cursor position for `wl_pointer.enter` and `wl_pointer.motion` events.

#### Scenario: Relative-only cursor updates
- **WHEN** the user moves the mouse with the UI overlay hidden
- **THEN** the compositor cursor updates using raw relative motion deltas
- **AND** absolute viewer coordinates are ignored

### Requirement: Virtual Keyboard Device

The system SHALL create a virtual keyboard device for input delivery.

The virtual keyboard SHALL:
- Be created via `wlr_keyboard_init()` from the keyboard interface
- Have an xkb keymap configured via `wlr_keyboard_set_keymap()`
- Be attached to the seat via `wlr_seat_set_keyboard()`

#### Scenario: Virtual keyboard provides keymap to clients
- **WHEN** a Wayland client connects and binds wl_keyboard
- **THEN** the client receives the keyboard's xkb keymap
- **AND** key events use keycodes consistent with the keymap

### Requirement: No X11/XTest Dependencies

The input forwarding module SHALL NOT depend on X11 or XTest libraries for input injection.

All input SHALL be delivered through the unified `wlr_seat_*` APIs, with wlr_xwm handling X11 translation for XWayland surfaces.

#### Scenario: Input works without X11 client connection
- **WHEN** input events are forwarded
- **THEN** no X11 Display connection is opened by the input forwarder
- **AND** no XTest extension calls are made
- **AND** wlr_xwm handles all X11 protocol translation internally

### Requirement: Popup Surface Support

The system SHALL support Wayland `xdg_popup` surfaces associated with a mapped `xdg_toplevel`.

The compositor server SHALL:
- Listen for `new_popup` signals from `xdg_shell`
- Track popup surfaces separately from toplevel surfaces with parent linkage and stacking order
- Send initial configure for popups and respect ack_configure before focus changes
- Render popups above their parent surface in the presented frame
- Route pointer and keyboard events to the topmost mapped popup while a popup grab is active

#### Scenario: Wayland menu popup renders
- **GIVEN** a Wayland toplevel is mapped
- **WHEN** the client creates and maps an `xdg_popup` (menu/dropdown)
- **THEN** the popup is configured and rendered above the parent surface
- **AND** input is delivered to the popup until it is dismissed

#### Scenario: Popup dismissed with parent
- **GIVEN** a parent toplevel with a mapped popup
- **WHEN** the parent surface is destroyed
- **THEN** all associated popups are removed from tracking

### Requirement: XWayland Override-Redirect Popups

The system SHALL present XWayland override-redirect surfaces (menus/tooltips) as popups.

The compositor server SHALL:
- Track override-redirect XWayland surfaces as transient popups
- Accept map requests for override-redirect surfaces
- Render override-redirect surfaces above the focused XWayland surface
- Route pointer and keyboard events to the topmost mapped override-redirect surface while visible

#### Scenario: X11 menu popup renders
- **GIVEN** an XWayland surface is focused
- **WHEN** the app creates an override-redirect menu window
- **THEN** the menu is mapped and rendered above the parent surface
- **AND** pointer clicks are delivered to the menu window

### Requirement: Relative Pointer Motion

The system SHALL support relative pointer motion via the `zwp_relative_pointer_v1` Wayland protocol extension.

The implementation SHALL:
- Create a `wlr_relative_pointer_manager_v1` on the Wayland display
- Send relative motion events via `wlr_relative_pointer_manager_v1_send_relative_motion()` for all pointer motion
- Forward SDL's `xrel/yrel` motion deltas to the compositor
- Send both relative and absolute motion for each pointer event (gamescope pattern)

#### Scenario: Relative motion forwarded to FPS game
- **WHEN** user moves mouse with relative delta (dx=10, dy=-5)
- **THEN** both `wl_pointer.motion` and `zwp_relative_pointer_v1.relative_motion` events are sent
- **AND** the client receives raw, unaccelerated deltas for mouselook

#### Scenario: Relative motion works without absolute position
- **WHEN** a game uses only relative pointer protocol (mouselook mode)
- **THEN** mouse movement translates to view rotation
- **AND** no cursor position is displayed or tracked

### Requirement: Pointer Constraints

The system SHALL support pointer lock and confine via the `zwp_pointer_constraints_v1` Wayland
protocol extension.

The implementation SHALL:
- Create a `wlr_pointer_constraints_v1` manager on the Wayland display.
- Listen for `new_constraint` signals from clients.
- Listen for `set_region` signals to refresh confinement bounds.
- Activate constraints on the focused surface automatically.
- Deactivate constraints when focus changes to a different surface.
- Support both `locked_pointer` (cursor disappears) and `confined_pointer` (cursor stays in region).
- Send `activated`/`deactivated` events to inform clients of constraint state.
- Apply confinement using the constraint region in surface coordinates.
- Apply cursor hints when provided for locked constraints.

#### Scenario: Pointer lock activated by game
- **WHEN** a game requests pointer lock via `zwp_pointer_constraints_v1.lock_pointer`
- **AND** the game's surface has focus
- **THEN** the constraint is activated
- **AND** relative motion events continue to be sent
- **AND** absolute cursor position is not updated

#### Scenario: Pointer lock uses cursor hint
- **WHEN** a locked pointer provides a cursor hint
- **THEN** the compositor cursor position is updated to the hinted location
- **AND** absolute pointer motion remains suppressed while locked

#### Scenario: Pointer confine region updates
- **WHEN** a client updates the pointer confine region
- **THEN** the compositor clamps cursor motion to the new region

#### Scenario: Pointer confine restricts cursor
- **WHEN** a client requests pointer confine to a region
- **AND** user moves cursor toward the region boundary
- **THEN** cursor motion is clamped to stay within the region
- **AND** motion events reflect the clamped position

### Requirement: Extended Button Support

The system SHALL forward all mouse button events, not limited to left/middle/right.

The implementation SHALL:
- Map SDL button codes to Linux `input-event-codes.h` button constants
- Support side buttons: `BTN_SIDE` (X1), `BTN_EXTRA` (X2)
- Support forward/back buttons: `BTN_FORWARD`, `BTN_BACK`
- Support task button: `BTN_TASK`
- Pass through unmapped buttons using `BTN_MISC` + offset as fallback

#### Scenario: Side button forwarded correctly
- **WHEN** user presses mouse side button (X1)
- **THEN** `BTN_SIDE` (0x113) is forwarded to the focused surface
- **AND** the client receives the button event

#### Scenario: Unmapped button passed through
- **WHEN** user presses an uncommon button not in standard mapping
- **THEN** the button is forwarded as `BTN_MISC` + button offset
- **AND** logging indicates the fallback mapping

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

### Requirement: Wlroots Logging Bridge
The compositor server SHALL route wlroots log output through the project logging utilities and respect configured verbosity.

#### Scenario: Default log level filters wlroots debug noise
- **GIVEN** the application log level is info (default)
- **WHEN** the compositor initializes wlroots logging
- **THEN** wlroots debug logs are suppressed
- **AND** wlroots info/error logs are emitted through the project logger

#### Scenario: Debug level exposes wlroots diagnostics
- **GIVEN** the application log level is debug or trace
- **WHEN** wlroots emits a debug log message
- **THEN** the message is emitted via the project logger at debug level

### Requirement: Stderr Suppression Does Not Hide Wlroots Logs
The compositor server SHALL NOT suppress wlroots logs when stderr suppression is active for external helper noise.

#### Scenario: External stderr suppression enabled
- **GIVEN** stderr suppression is enabled to reduce external helper noise
- **WHEN** wlroots emits an error log
- **THEN** the error is still visible via the project logger

### Requirement: Compositor Public API

The system SHALL expose input forwarding and compositor-presented surface frames via the `CompositorServer` public API, without requiring a separate forwarding wrapper.

#### Scenario: Application integrates compositor directly
- **WHEN** the application initializes input forwarding
- **THEN** it creates and owns a `CompositorServer`
- **AND** it forwards SDL input events via `CompositorServer` methods
- **AND** it can query compositor-presented surface frames from the same instance

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

### Requirement: Compositor Software Cursor

The system SHALL render a software cursor inside compositor-presented frames for the focused
surface.

The compositor server SHALL:
- Track cursor position in surface-local coordinates.
- Render the cursor overlay into the compositor-presented frame buffer.
- Hide the software cursor when pointer lock is active or when input forwarding is suspended.
- Load cursor imagery from the shipped Xcursor assets in `assets/cursor`.

#### Scenario: Cursor visible for compositor surface
- **WHEN** the compositor is presenting a surface frame
- **AND** the focused surface does not hold a pointer lock
- **THEN** a software cursor is rendered into the presented frame
- **AND** the cursor position matches the compositor cursor coordinates used for pointer events

#### Scenario: Cursor hidden during pointer lock
- **WHEN** a focused surface activates `zwp_pointer_constraints_v1.lock_pointer`
- **THEN** the software cursor is hidden
- **AND** only relative pointer events continue to be delivered

#### Scenario: Cursor hidden while UI overlay is visible
- **WHEN** the viewer UI overlay is visible
- **THEN** pointer events are not forwarded to the focused surface
- **AND** the compositor software cursor is hidden

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

#### Scenario: User docks overlay windows
- **WHEN** Goggles Overlay is visible
- **AND** user drags "Shader Controls" onto "Application"
- **THEN** both windows become docked in a shared tab stack
- **AND** the docking layout persists in imgui.ini for the next launch

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

