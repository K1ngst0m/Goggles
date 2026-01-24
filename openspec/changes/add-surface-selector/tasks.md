## 1. Surface Metadata and Tracking

- [x] 1.1 Add `SurfaceInfo` struct to `compositor_server.hpp` with: `id`, `title`, `class_name`, `width`, `height`, `is_xwayland`, `is_input_target`
- [x] 1.2 Add `uint32_t` surface ID counter to `CompositorServer::Impl`
- [x] 1.3 Add `id` field to `XWaylandSurfaceHooks` and `XdgToplevelHooks`
- [x] 1.4 Assign unique ID when surface is created
- [x] 1.5 Add `std::vector<SurfaceInfo>` cache updated on surface add/remove/focus change

## 2. X11 Metadata Retrieval

- [x] 2.1 Add helper function `get_x11_window_title(wlr_xwayland_surface*)` using `WM_NAME` atom
- [x] 2.2 Add helper function `get_x11_window_class(wlr_xwayland_surface*)` using `WM_CLASS` atom
- [x] 2.3 Query title/class in `handle_xwayland_surface_associate()` when X11 window is bound
- [x] 2.4 Store title/class in `XWaylandSurfaceHooks`

## 3. Surface Enumeration API

- [x] 3.1 Add `[[nodiscard]] auto get_surfaces() const -> std::vector<SurfaceInfo>` to `CompositorServer`
- [x] 3.2 Implement by iterating tracked surfaces and building `SurfaceInfo` list
- [x] 3.3 Mark current input target in returned list

## 4. Manual Input Target Selection

- [x] 4.1 Add `std::optional<uint32_t> m_manual_input_target` to `Impl`
- [x] 4.2 Add `void set_input_target(uint32_t surface_id)` to `CompositorServer`
- [x] 4.3 Add `void clear_input_override()` to `CompositorServer`
- [x] 4.4 Modify focus logic: if manual target set, use it; otherwise use first surface
- [x] 4.5 Validate target ID exists before applying

## 5. InputForwarder Integration

- [x] 5.1 Add `get_surfaces()` forwarding method to `InputForwarder`
- [x] 5.2 Add `set_input_target(uint32_t)` forwarding method
- [x] 5.3 Add `clear_input_override()` forwarding method

## 6. ImGui Surface Selector Window

- [x] 6.1 Add `SurfaceSelectorState` struct to `imgui_layer.hpp` with surface list and selection state
- [x] 6.2 Add `m_surface_selector_visible` bool (default false)
- [x] 6.3 Add `toggle_surface_selector()` and `is_surface_selector_visible()` methods
- [x] 6.4 Add `set_surfaces(std::vector<SurfaceInfo>)` to update displayed list
- [x] 6.5 Add callback type for surface selection
- [x] 6.6 Implement `draw_surface_selector()` method

## 7. Surface Selector UI Implementation

- [x] 7.1 Draw window with "Surfaces" title
- [x] 7.2 List each surface: radio button, ID, title (or "XDG Surface N"), dimensions
- [x] 7.3 Highlight current input target
- [x] 7.4 Show "(auto)" or "(manual)" indicator for selection mode
- [x] 7.5 Add "Reset to Auto" button that calls `clear_input_override()`
- [x] 7.6 Invoke selection callback when surface clicked

## 8. Application Integration

- [x] 8.1 Add F4 key handler to toggle surface selector
- [x] 8.2 Poll `get_surfaces()` each frame when selector visible
- [x] 8.3 Update ImGui layer with surface list
- [x] 8.4 Connect selection callback to `InputForwarder::set_input_target()`
- [x] 8.5 Connect reset callback to `InputForwarder::clear_input_override()`

## 9. Testing and Validation

- [x] 9.1 Run `pixi run dev -p quality` and fix any issues
- [x] 9.2 Manual test: launch app, verify surface appears in list
- [x] 9.3 Manual test: click surface to select, verify input routes correctly
- [x] 9.4 Manual test: "Reset to Auto" restores first-surface behavior
- [x] 9.5 Manual test with Steam: verify overlay surfaces appear when invoked
- [x] 9.6 Multi-surface test: Run `pixi run start -- goggles_manual_surface_selector_x11`, verify 3 windows appear in F4 selector
- [x] 9.7 Multi-surface test: Select each surface in F4 UI, verify input routes to correct window
- [x] 9.8 Multi-surface test: Click "Reset to Auto", verify first-surface behavior restored
- [x] 9.9 Multi-surface test: Close one window, verify selector updates correctly
- [x] 9.10 Repeat 9.6-9.9 with `goggles_manual_surface_selector_wayland`
