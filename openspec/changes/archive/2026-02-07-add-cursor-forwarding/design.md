## Context

Games and mouse-captured applications require more sophisticated input handling than simple absolute coordinate forwarding. The Wayland ecosystem provides two key protocol extensions for this:
- `zwp_relative_pointer_v1` - raw, unaccelerated mouse deltas for mouselook/camera control
- `zwp_pointer_constraints_v1` - cursor lock and confine for keeping cursor within window

Both protocols are implemented via wlroots and widely supported by game clients (including Wine/Proton).

## Goals / Non-Goals

**Goals:**
- Enable FPS games and mouselook applications to work correctly
- Support cursor lock/confine for games that capture the mouse
- Forward all mouse buttons (not just left/middle/right)
- Follow gamescope's proven implementation patterns

**Non-Goals:**
- Touch input support (separate proposal)
- High-resolution scroll (v120 protocol, separate proposal)
- Tablet/stylus input (future work)
- Coordinate scaling/mapping between windows (existing limitation documented)

## Decisions

### Decision: Send both relative and absolute motion

Following gamescope's pattern, every pointer motion event sends:
1. Relative motion via `wlr_relative_pointer_manager_v1_send_relative_motion()`
2. Absolute motion via `wlr_seat_pointer_notify_motion()`

**Rationale:** Wayland clients may bind to either or both protocols. Native Wayland games often use relative pointer, while X11 games via XWayland need absolute motion for wlr_xwm translation.

**Alternatives considered:**
- Only send relative when client binds relative_pointer - more complex tracking, breaks some games
- Only send absolute - current behavior, breaks FPS games

### Decision: Automatic constraint activation on focused surface

When a client requests a pointer constraint, we automatically activate it if the requesting surface has focus.

**Rationale:** Simplifies implementation and matches user expectation - when you click in a game, it should capture the cursor.

### Decision: Use InputEvent struct for relative deltas

Extend existing `InputEvent` with `dx`/`dy` fields rather than creating a separate relative motion event type.

**Rationale:** Keeps the event struct compact and allows a single motion event to carry both absolute and relative data, matching how SDL delivers them.

## Risks / Trade-offs

- **Constraint region math**: Confine constraints require region intersection. Initial implementation may only support full-surface confine, with region support added later.
- **Focus race conditions**: Constraint activation/deactivation during rapid focus changes could cause brief input glitches. Mitigate by clearing constraint state atomically with focus changes.

## Migration Plan

No migration needed - this is additive functionality. Existing applications continue to work unchanged.

## Open Questions

1. Should we expose constraint state to the InputForwarder API for UI feedback (e.g., showing lock icon)?
2. Do we need to handle persistent constraints that survive focus loss (oneshot vs persistent)?
