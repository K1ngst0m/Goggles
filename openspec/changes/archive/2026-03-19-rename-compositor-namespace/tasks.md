# Tasks: Rename compositor namespace from goggles::input to goggles::compositor

- [x] T1 Rename namespace declarations in all compositor module files
  - Description: Replace every `namespace goggles::input` declaration with `namespace goggles::compositor` across all 14 source files in `src/compositor/`. This covers both the opening declaration and any closing comment annotations. No type names, function signatures, or class APIs change.
  - Files involved: `src/compositor/compositor_server.hpp`, `src/compositor/compositor_server.cpp`, `src/compositor/compositor_state.hpp`, `src/compositor/compositor_protocol_hooks.hpp`, `src/compositor/compositor_targets.hpp`, `src/compositor/compositor_runtime_metrics.hpp`, `src/compositor/compositor_core.cpp`, `src/compositor/compositor_cursor.cpp`, `src/compositor/compositor_input.cpp`, `src/compositor/compositor_focus.cpp`, `src/compositor/compositor_layer_shell.cpp`, `src/compositor/compositor_present.cpp`, `src/compositor/compositor_xdg.cpp`, `src/compositor/compositor_xwayland.cpp`
  - Verification method: `rg -n "goggles::input" src/compositor/` returns zero matches; `pixi run build -p debug` succeeds
  - Dependencies: None
  - Estimated complexity: S

- [x] T2 Update forward declaration in UI module
  - Description: Update the forward declaration `namespace goggles::input { struct SurfaceInfo; }` to `namespace goggles::compositor { struct SurfaceInfo; }` in the UI module header.
  - Files involved: `src/ui/imgui_layer.hpp`
  - Verification method: `rg -n "goggles::input" src/ui/` returns zero matches
  - Dependencies: T1
  - Estimated complexity: S

- [x] T3 Update qualified references in test files
  - Description: Replace all `goggles::input::` qualified references with `goggles::compositor::` in the three test files that consume compositor types. This includes `goggles::input::CompositorServer::create()` in both input-forwarding tests and `goggles::input::wlr_surface*` / `goggles::input::RuntimeMetricsState` in the filter boundary contracts test.
  - Files involved: `tests/input/auto_input_forwarding_wayland.cpp`, `tests/input/auto_input_forwarding_x11.cpp`, `tests/render/test_filter_boundary_contracts.cpp`
  - Verification method: `rg -n "goggles::input" tests/` returns zero matches; `pixi run build -p debug` succeeds
  - Dependencies: T1
  - Estimated complexity: S

- [x] T4 Format and full CI verification
  - Description: Run the formatter to normalize any style drift introduced by the rename, then run the full CI pipeline to confirm the entire codebase compiles, links, passes all tests, and satisfies static analysis with the new namespace.
  - Files involved: CI output only
  - Verification method: `pixi run format`; `pixi run ci --runner container --cache-mode warm --lane all` exits with code 0
  - Dependencies: T1, T2, T3
  - Estimated complexity: S

- [x] T5 Final grep sweep
  - Description: Confirm zero occurrences of the old namespace remain anywhere in the source and test trees. This is the terminal acceptance gate for the rename.
  - Files involved: `src/`, `tests/`
  - Verification method: `rg -n "goggles::input" src/ tests/` returns empty output (no matches)
  - Dependencies: T4
  - Estimated complexity: S
