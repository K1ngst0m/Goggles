## Context

Goggles exposes a filter-chain C ABI in `src/render/chain/include/goggles_filter_chain.h` and currently consumes that header directly in internal C++ runtime code (notably backend integration). The goggles-interview plan requires a C++20-native API surface for internal/public C++ usage while preserving the C ABI boundary and C contract tests.

Policy constraints from `docs/project_policies.md` apply:

- Expected runtime failures MUST use `Result`-style propagation, not exceptions.
- App Vulkan code MUST keep existing `vk::` conventions.
- Ownership and lifetime must remain explicit and RAII-friendly.

Wave 1 is intentionally narrow: migrate runtime lifecycle/runtime callsites while preserving ABI boundary files and C ABI contract tests.

## Goals / Non-Goals

**Goals:**

- Provide a C++20 wrapper header for filter-chain lifecycle/runtime operations with strong typing and RAII ownership.
- Remove direct internal runtime includes/usages of `goggles_filter_chain.h` in wave-1 scope.
- Keep C ABI behavior and test coverage stable.
- Enforce no C-style surface leakage in migrated C++ callsites.

**Non-Goals:**

- Full C API parity in wave 1 (advanced controls/snapshot APIs may remain for wave 2).
- Removing or changing the C ABI contract.
- Reworking unrelated render pipeline behavior.

## Decisions

### 1) C++20 wrapper shape is value/RAII-first

Decision:

- Introduce a dedicated C++ wrapper header in render chain include surface.
- Expose a move-only chain handle type that owns the underlying `goggles_chain_t*` and destroys it in destructor.
- Provide lifecycle/runtime operations for wave 1: create, preset load, resize, record, stage policy get/set.

Rationale:

- Eliminates pointer-to-pointer lifecycle patterns at C++ callsites.
- Makes ownership and destruction deterministic.

Alternatives considered:

- Thin free-function wrapper around C ABI: rejected (still C-shaped API ergonomics).
- Full API parity in wave 1: deferred to reduce risk and migration churn.

### 2) Error model mirrors project policy (`Result`, no exception-based expected failures)

Decision:

- Every fallible wrapper operation MUST return project `Result` (`tl::expected<T, Error>` or project alias); sentinel-only status-object APIs are not permitted for wrapper-facing operations.
- Wrapper APIs MUST NOT require exception handling for expected runtime failures.
- Errors MUST be handled or propagated, and logging MUST occur at subsystem boundaries without duplicate cascading logs.

Rationale:

- Aligns with repo-wide error handling policy and callsite patterns.

Alternatives considered:

- Exception-based wrapper API: rejected by policy and consistency requirements.

### 3) Strongly typed enums/flags replace raw integer protocol in C++ surface

Decision:

- Define C++ enum classes / typed flag wrappers for stage and scale modes in wrapper-facing API.
- Hide C macro/status constants from normal C++ consumer paths.
- Wrapper-facing Vulkan signatures in app/runtime C++ paths MUST use `vk::` types and MUST NOT expose raw `Vk*` handles.

Rationale:

- Improves misuse resistance and readability.

Alternatives considered:

- Re-export C integer constants in C++ namespace: rejected as insufficiently typed.

### 4) Boundary isolation is strict

Decision:

- C header remains only at ABI boundary implementation and C ABI contract tests in wave 1.
- Internal runtime code MUST include/use the C++ wrapper, not the C header.
- The wrapper header MUST NOT require direct runtime inclusion of `goggles_filter_chain.h` by C++ consumer modules.

Rationale:

- Preserves ABI while enabling C++ API evolution.

Alternatives considered:

- Temporary mixed runtime includes: rejected as it weakens migration guarantees.

### 5) Blocker fallback uses adapter/shim, never runtime boundary regression

Decision:

- If hidden blocker callsites appear, introduce adapter/shim at boundary-facing edges while keeping runtime C++ wrapper contract intact.
- Direct reintroduction of raw C-header runtime use is not allowed.

Rationale:

- Maintains architectural direction under delivery pressure.

Alternatives considered:

- Temporary waiver to direct C-header runtime usage: rejected.

### 6) Stage-order invariants are preserved

Decision:

- Wrapper stage policy APIs only control stage enablement and MUST preserve execution order invariants: `pre-chain -> effect chain -> output pass`.

Rationale:

- Prevents API migration from altering render pipeline semantics.

Alternatives considered:

- Allowing stage reordering through wrapper control: rejected as out of scope and incompatible with pipeline invariants.

### 7) Enforcement strategy is explicit for wave 1

Decision:

- Wave-1 enforcement SHALL use concrete command checks and test gates defined in `tasks.md` (include-boundary grep checks, policy grep checks, and preset-driven `ctest`/Pixi verification).
- A separate custom lint target is deferred unless wave-1 verification proves insufficient.

Rationale:

- Keeps enforcement deterministic for `/goggles-apply` without adding tooling scope in this change.

Alternatives considered:

- Introduce a dedicated lint target in wave 1: deferred to avoid expanding this proposal beyond wrapper migration scope.

## Risks / Trade-offs

- [Risk] Wrapper drifts into naming shim only -> Mitigation: spec-level requirement for typed/RAII/no-leakage API and review checks.
- [Risk] Hidden callsites increase migration scope -> Mitigation: explicit wave-1 scope lock + adapter/shim fallback rule.
- [Risk] Behavioral drift vs C ABI -> Mitigation: preserve C ABI contract tests and require parity checks in wave-1 validation.
- [Trade-off] Deferring advanced APIs to wave 2 leaves temporary dual-surface state -> Mitigation: explicitly document deferred scope and required follow-up.

## Migration Plan

1. Add wrapper header and minimal support plumbing for lifecycle/runtime operations.
2. Switch wave-1 runtime callsites from direct C header to wrapper surface.
3. Keep ABI boundary and C API contract tests unchanged.
4. Add include-boundary verification checks and run required build/test presets.
5. If blocker callsites are discovered, apply boundary-safe adapter/shim first and continue migration without runtime boundary regression.
6. If behavior/spec divergence is still required after adapter/shim attempt, halt apply work and reconcile proposal/design/spec artifacts before further implementation.

Rollback strategy:

- Revert wrapper adoption in affected runtime callsites while preserving unchanged C ABI contract behavior.
- Do not remove/alter C ABI files in rollback.

## Open Questions

- Which advanced APIs (controls/snapshots/diagnostics) should be promoted in wave 2 first?
