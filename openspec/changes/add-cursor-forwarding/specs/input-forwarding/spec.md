## ADDED Requirements

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

The system SHALL support pointer lock and confine via the `zwp_pointer_constraints_v1` Wayland protocol extension.

The implementation SHALL:
- Create a `wlr_pointer_constraints_v1` manager on the Wayland display
- Listen for `new_constraint` signals from clients
- Activate constraints on the focused surface automatically
- Deactivate constraints when focus changes to a different surface
- Support both `locked_pointer` (cursor disappears) and `confined_pointer` (cursor stays in region)
- Send `activated`/`deactivated` events to inform clients of constraint state

#### Scenario: Pointer lock activated by game
- **WHEN** a game requests pointer lock via `zwp_pointer_constraints_v1.lock_pointer`
- **AND** the game's surface has focus
- **THEN** the constraint is activated
- **AND** relative motion events continue to be sent
- **AND** absolute cursor position is not updated (or updated to hint position)

#### Scenario: Pointer lock released on focus loss
- **WHEN** focus changes from a surface with active pointer lock
- **THEN** the constraint is deactivated
- **AND** the client receives `zwp_locked_pointer_v1.unlocked` event

#### Scenario: Pointer confine restricts cursor
- **WHEN** a client requests pointer confine to a region
- **AND** user moves cursor toward region boundary
- **THEN** cursor position is clamped to stay within the confine region
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

## MODIFIED Requirements

### Requirement: Unified Pointer Input

The system SHALL forward pointer events (motion, button, axis) to the focused surface using `wlr_seat_pointer_*` APIs.

The implementation SHALL:
- Use `wlr_seat_pointer_enter()` on surface focus
- Use `wlr_seat_pointer_notify_motion()` for absolute motion
- Use `wlr_seat_pointer_notify_button()` for all button events (extended set)
- Use `wlr_seat_pointer_notify_axis()` for scroll events
- Use `wlr_seat_pointer_notify_frame()` to group related events
- **Send relative motion via `wlr_relative_pointer_manager_v1_send_relative_motion()` alongside absolute motion**
- **Respect active pointer constraints when processing motion**

The wlr_xwm SHALL automatically translate pointer events to X11 for XWayland surfaces.

#### Scenario: Mouse motion forwarded to Wayland client
- **WHEN** user moves mouse in the viewer window
- **AND** a Wayland surface has pointer focus
- **THEN** the motion event is delivered via wl_pointer.motion protocol
- **AND** relative motion is delivered via zwp_relative_pointer_v1 if client bound it

#### Scenario: Mouse motion forwarded to X11 client
- **WHEN** user moves mouse in the viewer window
- **AND** an XWayland surface has pointer focus
- **THEN** wlr_xwm translates the event to X11 MotionNotify
- **AND** the X11 app receives the event

#### Scenario: Motion respects pointer lock
- **WHEN** user moves mouse with active pointer lock
- **THEN** relative motion is sent to the client
- **AND** absolute cursor position is not changed
- **AND** no wl_pointer.motion is sent (only relative)
