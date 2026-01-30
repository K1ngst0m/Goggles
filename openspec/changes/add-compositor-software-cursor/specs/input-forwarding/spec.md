## ADDED Requirements

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

## MODIFIED Requirements

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
