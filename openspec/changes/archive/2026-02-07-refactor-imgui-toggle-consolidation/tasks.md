## 1. Application Window

- [x] 1.1 Rename "Application Management" to "Application"
- [x] 1.2 Move debug overlay content (FPS graphs, performance stats) into Application as "Performance" section
- [x] 1.3 Change "Force Disable Pointer Lock" to "Force Enable Pointer Lock"
- [x] 1.4 Add hint when pointer lock enabled showing Ctrl+Alt+Shift+Q shortcut
- [x] 1.5 Keep surface selector in Application window (as "Input" section)

## 2. Shader Controls Window

- [x] 2.1 Keep only Pre-Chain, Effect, and Post-Chain stages
- [x] 2.2 No application or view controls

## 3. ImGui Layer Changes

- [x] 3.1 Add `m_global_visible` master visibility flag
- [x] 3.2 Rename `toggle_visibility()` to `toggle_global_visibility()`
- [x] 3.3 Update `begin_frame()` to skip all drawing when `!m_global_visible`
- [x] 3.4 Remove `draw_debug_overlay()` function and `m_debug_overlay_visible` member
- [x] 3.5 Windows are dockable (ImGuiConfigFlags_DockingEnable already set)

## 4. Application Changes

- [x] 4.1 Remove F1-F4 key handlers
- [x] 4.2 Add Ctrl+Alt+Shift+Q handler calling `toggle_global_visibility()`
- [x] 4.3 Fix modifier check to use OR logic (any Ctrl + any Alt + any Shift)
- [x] 4.4 Update pointer lock logic: `override || is_locked` (force enable)

## 5. Testing

- [x] 5.1 Verify Ctrl+Alt+Shift+Q toggles all UI visibility (works repeatedly)
- [x] 5.2 Verify F1-F4 keys pass through to target application
- [x] 5.3 Verify Performance section in Application shows FPS stats
- [x] 5.4 Verify Force Enable Pointer Lock checkbox works
- [x] 5.5 Verify hint appears when pointer lock enabled
- [x] 5.6 Verify surface selector works
- [x] 5.7 Verify collapsing headers work normally
- [x] 5.8 Test with game that uses F1-F4 keys
