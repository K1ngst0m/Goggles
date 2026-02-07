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

- [x] 3.1 Add `dx`, `dy` fields to `InputEvent` struct for relative deltas
- [x] 3.2 Update `inject_pointer_motion()` to accept dx/dy parameters
- [x] 3.3 Update `process_input_events()` to call `wlr_relative_pointer_manager_v1_send_relative_motion()`
- [x] 3.4 Modify absolute motion handling to also send relative motion (gamescope pattern)

## 4. Extended Button Support

- [x] 4.1 Extend `sdl_to_linux_button()` to map buttons 6+ to BTN_FORWARD, BTN_BACK, BTN_TASK
- [x] 4.2 Handle unmapped buttons by passing through raw SDL button + BTN_MISC offset
- [x] 4.3 Add trace logging for button mapping

## 5. InputForwarder API Extension

- [x] 5.1 Update `forward_mouse_motion()` to also forward xrel/yrel as relative motion
- [x] 5.2 Update docstrings in header file

## 6. Testing and Validation

- [x] 6.1 Add key 1 toggle for pointer lock in manual tests
- [x] 6.2 Add key 2 toggle for mouse grab in manual tests
- [x] 6.3 Add key 3 state query in manual tests
- [x] 6.4 Test pointer lock: verify relative motion works, absolute frozen
- [x] 6.5 Test mouse grab: verify cursor confined to window

## 7. Viewer Pointer Lock Mirror

- [x] 7.1 Add `is_pointer_locked()` to CompositorServer
- [x] 7.2 Expose `is_pointer_locked()` through InputForwarder
- [x] 7.3 Add F3 toggle for pointer lock override in Application
- [x] 7.4 Poll and mirror pointer lock state each frame
- [x] 7.5 Auto-release lock when ImGui is shown (F1)
- [x] 7.6 Test pointer lock mirroring with captured app
