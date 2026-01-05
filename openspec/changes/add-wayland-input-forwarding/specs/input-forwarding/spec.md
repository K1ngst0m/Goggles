## ADDED Requirements

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
- Auto-focus the first connected surface (single-app model)
- Use mutual exclusion to manage focus between Wayland and XWayland surfaces

**Important**: XWayland surfaces SHALL NOT use destroy listeners. XWayland destroy signals fire at unpredictable times during normal X11 operation, causing input forwarding failures. Instead, stale XWayland pointers are cleared during focus transitions.

#### Scenario: Wayland client connects and receives focus
- **WHEN** a Wayland client creates an xdg_toplevel
- **THEN** the surface is tracked by the compositor
- **AND** if no surface was previously focused, the new surface receives keyboard and pointer focus
- **AND** if an XWayland surface had focus, the Wayland surface steals focus (XWayland pointer may be stale)

#### Scenario: XWayland client connects and receives focus
- **WHEN** an X11 app creates a window via XWayland
- **THEN** the XWayland surface is tracked by the compositor (not in m_surfaces list)
- **AND** if no surface was previously focused, the surface receives keyboard and pointer focus
- **AND** if a Wayland surface already has focus, the XWayland surface does NOT steal focus

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

The system SHALL forward pointer events (motion, button, axis) to the focused surface using `wlr_seat_pointer_*` APIs.

The implementation SHALL:
- Use `wlr_seat_pointer_enter()` on surface focus
- Use `wlr_seat_pointer_notify_motion()` for motion
- Use `wlr_seat_pointer_notify_button()` for button events
- Use `wlr_seat_pointer_notify_axis()` for scroll events
- Use `wlr_seat_pointer_notify_frame()` to group related events

The wlr_xwm SHALL automatically translate pointer events to X11 for XWayland surfaces.

#### Scenario: Mouse motion forwarded to Wayland client
- **WHEN** user moves mouse in the viewer window
- **AND** a Wayland surface has pointer focus
- **THEN** the motion event is delivered via wl_pointer.motion protocol

#### Scenario: Mouse motion forwarded to X11 client
- **WHEN** user moves mouse in the viewer window
- **AND** an XWayland surface has pointer focus
- **THEN** wlr_xwm translates the event to X11 MotionNotify
- **AND** the X11 app receives the event

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

The system SHALL forward pointer coordinates without transformation.

Note: Coordinate mapping between viewer and target window dimensions is not implemented. Coordinates are passed through 1:1.

#### Scenario: Raw coordinate passthrough
- **WHEN** pointer motion occurs at position (x, y) in viewer
- **THEN** position (x, y) is forwarded to the target
- **AND** no scaling or transformation is applied

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
