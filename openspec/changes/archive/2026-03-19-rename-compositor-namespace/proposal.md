# Proposal: Rename compositor namespace from goggles::input to goggles::compositor

## Problem

The compositor module (`src/compositor/`) uses `goggles::input` as its C++ namespace. Every other module follows the project convention that namespace matches directory name (`app` → `goggles::app`, `render` → `goggles::render`, `ui` → `goggles::ui`, `util` → `goggles::util`). The compositor is the sole exception, affecting 14 source files internally and 4 files externally. This violates the ALWAYS rule "Follow namespace convention: `goggles::{module_name}`" (PROJECT_RULES.md, I3) and was confirmed as a historical anomaly in RFC.md (I5, Q1 — resolved: rename).

## Intent

Rename the `goggles::input` namespace to `goggles::compositor` so the project-wide namespace convention holds uniformly with zero exceptions.

## Scope

### In Scope

- Rename all `namespace goggles::input` declarations in `src/compositor/` to `namespace goggles::compositor`.
- Update all qualified references (`goggles::input::*`) in consuming modules and tests.
- Update the forward declaration in `src/ui/imgui_layer.hpp`.
- Run `pixi run format` after changes.
- Verify with `pixi run ci --runner container --cache-mode warm --lane all`.

### Out of Scope

- Renaming or reorganizing the `src/compositor/` directory structure.
- Changing any type names, function signatures, or class APIs within the compositor module.
- Modifying the filter-chain submodule (confirmed: zero references to `goggles::input`).
- Updating archived openspec documents (historical records remain as-is).
- Any functional or behavioral changes to the compositor.

## Approach

This is a single-phase mechanical rename. No architectural changes, no phased rollout.

### Step 1: Namespace declarations (14 files)

Replace `namespace goggles::input` with `namespace goggles::compositor` in all compositor module files:

| File | Type |
|------|------|
| `src/compositor/compositor_server.hpp` | Public header |
| `src/compositor/compositor_server.cpp` | Implementation |
| `src/compositor/compositor_state.hpp` | Internal header |
| `src/compositor/compositor_protocol_hooks.hpp` | Internal header |
| `src/compositor/compositor_targets.hpp` | Internal header |
| `src/compositor/compositor_runtime_metrics.hpp` | Internal header |
| `src/compositor/compositor_core.cpp` | Implementation |
| `src/compositor/compositor_cursor.cpp` | Implementation |
| `src/compositor/compositor_input.cpp` | Implementation |
| `src/compositor/compositor_focus.cpp` | Implementation |
| `src/compositor/compositor_layer_shell.cpp` | Implementation |
| `src/compositor/compositor_present.cpp` | Implementation |
| `src/compositor/compositor_xdg.cpp` | Implementation |
| `src/compositor/compositor_xwayland.cpp` | Implementation |

### Step 2: Forward declaration (1 file)

Update `src/ui/imgui_layer.hpp`:
- `namespace goggles::input { struct SurfaceInfo; }` → `namespace goggles::compositor { struct SurfaceInfo; }`

### Step 3: Qualified references in tests (3 files)

Update all `goggles::input::` qualifications:

| File | Symbols referenced |
|------|-------------------|
| `tests/input/auto_input_forwarding_wayland.cpp` | `goggles::input::CompositorServer::create()` |
| `tests/input/auto_input_forwarding_x11.cpp` | `goggles::input::CompositorServer::create()` |
| `tests/render/test_filter_boundary_contracts.cpp` | `goggles::input::wlr_surface*`, `goggles::input::RuntimeMetricsState` |

### Step 4: Format and verify

1. `pixi run format`
2. `pixi run ci --runner container --cache-mode warm --lane all`

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/compositor/*.hpp` | Modified | Namespace declaration changed in 6 headers |
| `src/compositor/*.cpp` | Modified | Namespace declaration changed in 8 implementation files |
| `src/ui/imgui_layer.hpp` | Modified | Forward declaration updated |
| `tests/input/auto_input_forwarding_*.cpp` | Modified | Qualified name references updated (2 files) |
| `tests/render/test_filter_boundary_contracts.cpp` | Modified | Qualified name references updated |
| `filter-chain/` | None | Zero references to `goggles::input` — no changes needed |
| CMake build files | None | No namespace-dependent configuration exists |

**Total: 18 files modified, 0 files created or deleted.**

## Non-goals

- Introduce a new `compositor` sub-namespace hierarchy or reorganize types within the module.
- Rename or split any types (e.g., keeping `InputEventType` as-is — the type name is semantic, not a namespace artifact).
- Update archived design documents that reference the old namespace.

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Missed reference causes build failure | Low | Full CI gate catches all compile errors; `goggles::input` does not exist after rename, so any remaining reference is a hard compile error |
| Symbol mangling change breaks something | None | Monolithic build; no binary distribution or ABI stability contract |
| Name collision with existing `goggles::compositor` | None | Confirmed: `goggles::compositor` does not exist anywhere in the codebase |

## Rollback Plan

Single commit. Revert the commit to restore `goggles::input` everywhere.

## Dependencies

None. No external consumers, no ABI boundary, no submodule impact.

## Validation Plan

- `pixi run build -p debug` — confirms compilation after rename.
- `pixi run test -p test` — confirms all tests pass with new namespace.
- `pixi run ci --runner container --cache-mode warm --lane all` — full CI gate (ALWAYS rule, I2/I7).

## Success Criteria

- [ ] Zero occurrences of `goggles::input` in `src/` and `tests/` (excluding archived docs).
- [ ] All compositor module files use `namespace goggles::compositor`.
- [ ] The forward declaration in `src/ui/imgui_layer.hpp` references `goggles::compositor`.
- [ ] Full CI passes: `pixi run ci --runner container --cache-mode warm --lane all`.
- [ ] The ALWAYS rule "Follow namespace convention: `goggles::{module_name}`" applies uniformly with no exceptions.

## PROJECT_RULES.md Compliance

| Rule | Status |
|------|--------|
| ALWAYS: Follow namespace convention `goggles::{module_name}` (I3) | This change enforces it |
| ALWAYS: Run `pixi run format` before committing (I7) | Step 4 |
| ALWAYS: Use `pixi run ci ...` for final verification (I2, I7) | Step 4 |
| NEVER: Create circular module dependencies (I8) | No dependency changes |
| NEVER: Modify files in `shaders/retroarch/` or `research/` (I3) | Not touched |
| ASK FIRST: Modifying filter-chain library boundary (I4) | Not touched — confirmed zero impact |
