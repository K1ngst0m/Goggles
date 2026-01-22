## 1. CompositorServer Protocol Setup

- [x] 1.1 Add `#include <wlr/types/wlr_relative_pointer_v1.h>` and `#include <wlr/types/wlr_pointer_constraints_v1.h>`
- [x] 1.2 Add `wlr_relative_pointer_manager_v1*` member to Impl struct
- [x] 1.3 Add `wlr_pointer_constraints_v1*` member to Impl struct
- [x] 1.4 Create relative pointer manager in `setup_input_devices()` via `wlr_relative_pointer_manager_v1_create()`
- [x] 1.5 Create pointer constraints in `setup_input_devices()` via `wlr_pointer_constraints_v1_create()`

## 2. Pointer Constraints Implementation

- [x] 2.1 Add constraint state tracking: `wlr_pointer_constraint_v1* active_constraint`, `wl_listener new_constraint_listener`
- [x] 2.2 Implement `handle_new_constraint()` callback to activate constraints on focused surface
- [x] 2.3 Implement `handle_constraint_destroy()` to clean up when constraint is released
- [x] 2.4 Add constraint activation when surface gains focus
- [x] 2.5 Add constraint deactivation when focus changes

## 3. Relative Pointer Motion

- [x] 3.1 Add `pointer_motion_relative` to `InputEventType` enum
- [x] 3.2 Add `dx`, `dy` fields to `InputEvent` struct for relative deltas
- [x] 3.3 Implement `inject_pointer_motion_relative(double dx, double dy)` method
- [x] 3.4 Update `process_input_events()` to call `wlr_relative_pointer_manager_v1_send_relative_motion()` for relative events
- [x] 3.5 Modify absolute motion handling to also send relative motion (gamescope pattern)

## 4. Extended Button Support

- [x] 4.1 Extend `sdl_to_linux_button()` to map buttons 6+ to BTN_FORWARD, BTN_BACK, BTN_TASK
- [x] 4.2 Handle unmapped buttons by passing through raw SDL button + BTN_MISC offset
- [x] 4.3 Add trace logging for button mapping

## 5. InputForwarder API Extension

- [x] 5.1 Update `forward_mouse_motion()` to also forward xrel/yrel as relative motion
- [x] 5.2 Update docstrings in header file

## 6. Testing and Validation

- [ ] 6.1 Test with FPS game that requires mouselook (e.g., wine/proton game)
- [ ] 6.2 Test pointer lock with application that captures cursor
- [ ] 6.3 Verify extended buttons work with multi-button mouse
- [ ] 6.4 Test focus changes properly activate/deactivate constraints
