# Spec: Rename compositor namespace from goggles::input to goggles::compositor

**Change:** rename-compositor-namespace
**Proposal:** [proposal.md](proposal.md)

---

## Requirement: Namespace Declaration Convention

All source files in the `src/compositor/` directory SHALL declare their namespace as `namespace goggles::compositor`. No file in the compositor module SHALL use `namespace goggles::input` or any other namespace that does not match the directory name.

### Scenario: Compositor headers use correct namespace

- **GIVEN** the compositor public and internal headers:
  - `src/compositor/compositor_server.hpp`
  - `src/compositor/compositor_state.hpp`
  - `src/compositor/compositor_protocol_hooks.hpp`
  - `src/compositor/compositor_targets.hpp`
  - `src/compositor/compositor_runtime_metrics.hpp`
- **WHEN** namespace declarations in each file are inspected
- **THEN** every namespace declaration SHALL be `namespace goggles::compositor`
- **AND** zero occurrences of `namespace goggles::input` SHALL exist

### Scenario: Compositor implementation files use correct namespace

- **GIVEN** the compositor implementation files:
  - `src/compositor/compositor_server.cpp`
  - `src/compositor/compositor_core.cpp`
  - `src/compositor/compositor_cursor.cpp`
  - `src/compositor/compositor_input.cpp`
  - `src/compositor/compositor_focus.cpp`
  - `src/compositor/compositor_layer_shell.cpp`
  - `src/compositor/compositor_present.cpp`
  - `src/compositor/compositor_xdg.cpp`
  - `src/compositor/compositor_xwayland.cpp`
- **WHEN** namespace declarations in each file are inspected
- **THEN** every namespace declaration SHALL be `namespace goggles::compositor`
- **AND** zero occurrences of `namespace goggles::input` SHALL exist

---

## Requirement: No Residual References to Old Namespace

After the rename, zero occurrences of the string `goggles::input` SHALL exist anywhere in `src/` or `tests/`. This ensures no stale qualified references, using-declarations, or comments referencing the old namespace survive the rename.

### Scenario: Source tree contains no old namespace references

- **GIVEN** the directories `src/` and `tests/`
- **WHEN** a text search for the literal string `goggles::input` is executed across all files
- **THEN** zero matches SHALL be found

### Scenario: No residual using-declarations reference old namespace

- **GIVEN** the directories `src/` and `tests/`
- **WHEN** a text search for `using namespace goggles::input` or `namespace input = goggles::input` is executed
- **THEN** zero matches SHALL be found

---

## Requirement: Forward Declaration Consistency

The forward declaration of compositor types in consumer modules SHALL reference the `goggles::compositor` namespace, not `goggles::input`.

### Scenario: ImGui layer forward declaration updated

- **GIVEN** the file `src/ui/imgui_layer.hpp`
- **WHEN** forward declarations of compositor types are inspected
- **THEN** the forward declaration of `SurfaceInfo` SHALL appear within `namespace goggles::compositor`
- **AND** zero occurrences of `namespace goggles::input` SHALL exist in the file

---

## Requirement: Qualified Reference Consistency in Tests

All test files that reference compositor types via fully qualified names SHALL use the `goggles::compositor::` prefix.

### Scenario: Input forwarding test files use new namespace

- **GIVEN** the test files:
  - `tests/input/auto_input_forwarding_wayland.cpp`
  - `tests/input/auto_input_forwarding_x11.cpp`
- **WHEN** qualified references to `CompositorServer::create()` are inspected
- **THEN** every reference SHALL use `goggles::compositor::CompositorServer::create()`
- **AND** zero occurrences of `goggles::input::CompositorServer` SHALL exist

### Scenario: Filter boundary contract tests use new namespace

- **GIVEN** the file `tests/render/test_filter_boundary_contracts.cpp`
- **WHEN** qualified references to compositor types are inspected
- **THEN** references to `wlr_surface` and `RuntimeMetricsState` SHALL use the `goggles::compositor::` prefix
- **AND** zero occurrences of `goggles::input::` SHALL exist in the file

---

## Requirement: Build Verification

The project SHALL compile cleanly and pass all tests after the namespace rename. Because the old namespace `goggles::input` ceases to exist, any missed reference will produce a hard compile error — there is no risk of a silent partial rename.

### Scenario: Debug build succeeds

- **GIVEN** all namespace changes are applied
- **WHEN** `pixi run build -p debug` is executed
- **THEN** the build SHALL succeed with zero errors

### Scenario: Full test suite passes

- **GIVEN** all namespace changes are applied
- **WHEN** `pixi run test -p test` is executed
- **THEN** all tests SHALL pass

### Scenario: Full CI pipeline passes

- **GIVEN** all namespace changes are applied and formatted via `pixi run format`
- **WHEN** `pixi run ci --runner container --cache-mode warm --lane all` is executed
- **THEN** all CI lanes SHALL pass (format, build+test with ASAN, package install + consumer validation, semgrep, clang-tidy quality gate)

---

## Requirement: No Functional Changes

The rename SHALL be a purely mechanical transformation. No type names, function signatures, class APIs, or runtime behavior SHALL change. Only the enclosing namespace identifier changes from `input` to `compositor`.

### Scenario: Type names are preserved

- **GIVEN** the compositor module types: `CompositorServer`, `CompositorState`, `SurfaceInfo`, `RuntimeMetricsState`, and all other public and internal types
- **WHEN** their declarations are inspected after the rename
- **THEN** every type name SHALL be identical to its pre-rename name
- **AND** only the enclosing namespace SHALL differ

### Scenario: Function signatures are preserved

- **GIVEN** all public and internal function signatures in the compositor module
- **WHEN** they are inspected after the rename
- **THEN** every function name, parameter list, and return type SHALL be identical to pre-rename
- **AND** no function SHALL be added, removed, or modified

### Scenario: No files created or deleted

- **GIVEN** the set of files modified by this change
- **WHEN** the changeset is inspected
- **THEN** zero files SHALL be created
- **AND** zero files SHALL be deleted
- **AND** exactly 18 files SHALL be modified (14 compositor module files, 1 UI forward declaration, 3 test files)
