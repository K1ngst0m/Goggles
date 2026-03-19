# Design: Rename compositor namespace from goggles::input to goggles::compositor

## Technical Approach

This is a mechanical find-and-replace rename executed as a single atomic commit. Every occurrence of `namespace goggles::input` and every qualified `goggles::input::` reference is replaced with `goggles::compositor`. No phased rollout, no compatibility shims, no intermediate states.

The change touches three categories of files:

1. **Compositor source** (14 files): Replace `namespace goggles::input` declarations in all headers and implementation files under `src/compositor/`.
2. **UI forward declaration** (1 file): Update the forward declaration of `SurfaceInfo` in `src/ui/imgui_layer.hpp`.
3. **Test references** (3 files): Update qualified `goggles::input::` references in `tests/input/` and `tests/render/`.

Total: 18 files modified, 0 files created or deleted.

## Architecture Decisions

### Decision: Single atomic commit

**Choice**: Apply all 18 file changes in one commit.
**Alternatives considered**: Incremental migration with a namespace alias bridging old and new names across multiple commits.
**Rationale**: The rename is purely mechanical with no external consumers or ABI boundary. A single commit eliminates any intermediate state where both namespaces coexist. If the build breaks, the failure is a hard compile error (the old namespace ceases to exist), making missed references trivially detectable. Rollback is a single `git revert`.

### Decision: Type names unchanged

**Choice**: Keep all type names as-is (e.g., `InputEventType`, `CompositorServer`).
**Alternatives considered**: Rename types that contain "input" to use "compositor" for consistency.
**Rationale**: Type names like `InputEventType` are semantic names describing what they represent (input events), not artifacts of the namespace they happen to live in. Renaming them would be a separate semantic change with its own scope and risk, not part of a namespace alignment fix.

### Decision: Archived openspec documents not updated

**Choice**: Do not modify any existing openspec documents that reference `goggles::input`.
**Alternatives considered**: Update all historical references across the repository.
**Rationale**: Archived design documents are historical records. Retroactively editing them misrepresents what was designed at the time. The rename is self-documenting through this change's own openspec artifacts and the git history.

## Affected Components

### Compositor source files (14 files)

All files under `src/compositor/` that declare `namespace goggles::input`:

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

### UI forward declaration (1 file)

| File | Change |
|------|--------|
| `src/ui/imgui_layer.hpp` | `namespace goggles::input { struct SurfaceInfo; }` becomes `namespace goggles::compositor { struct SurfaceInfo; }` |

### Test references (3 files)

| File | Symbols referenced |
|------|-------------------|
| `tests/input/auto_input_forwarding_wayland.cpp` | `goggles::input::CompositorServer::create()` |
| `tests/input/auto_input_forwarding_x11.cpp` | `goggles::input::CompositorServer::create()` |
| `tests/render/test_filter_boundary_contracts.cpp` | `goggles::input::wlr_surface*`, `goggles::input::RuntimeMetricsState` |

### Unaffected areas

| Area | Why unaffected |
|------|---------------|
| `filter-chain/` | Zero references to `goggles::input` anywhere in the submodule |
| CMake build files | No namespace-dependent configuration exists |
| `shaders/retroarch/` | Read-only upstream mirror, no C++ references |
| `research/` | Read-only reference material |

## Data Flow

No data flow changes. The rename is purely syntactic. All runtime behavior, call graphs, data ownership, threading model, and module boundaries remain identical. The compiled binary is semantically equivalent (only symbol mangling changes, which is irrelevant since there is no binary distribution or ABI stability contract).

## Interfaces / Contracts

No interface or contract changes. Every public and internal API retains its signature, semantics, and behavior. The only difference is the enclosing namespace identifier in declarations and qualified references.

## Testing Strategy

| Layer | What to Test | Approach |
|-------|--------------|----------|
| Compilation | All 18 modified files compile | `pixi run build -p debug` confirms no remaining `goggles::input` references cause errors |
| Unit + integration tests | All existing tests pass | `pixi run test -p test` exercises the full test suite including the 3 modified test files |
| Full CI | Format, ASAN, clang-tidy, consumer validation, semgrep | `pixi run ci --runner container --cache-mode warm --lane all` is the single acceptance gate |

No new tests are needed. The rename does not introduce new behavior to verify. The existing test suite provides complete coverage of the renamed namespace through the 3 test files that reference compositor types.

## Verification Strategy

1. `pixi run format` — ensure clang-format compliance after edits.
2. `pixi run ci --runner container --cache-mode warm --lane all` — full CI pipeline as the sole acceptance gate per project policy (ALWAYS rule I2/I7).

A green full-CI run is the only accepted verification. Partial commands are useful during development but do not substitute for the final gate.

## Migration / Rollout

Not applicable. This is a single atomic commit with no migration path, no compatibility period, and no phased rollout. The old namespace ceases to exist the moment the commit lands.

**Rollback**: `git revert <commit-sha>` restores `goggles::input` everywhere.

## Open Questions

None. All decisions are resolved in the proposal. The rename is fully mechanical with no ambiguity.
