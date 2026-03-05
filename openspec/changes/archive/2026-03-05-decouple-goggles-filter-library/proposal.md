## Why

The current filter chain is tightly coupled to backend lifecycle and app/UI control paths, which makes it hard to test and evolve independently. We need a clear extraction boundary so filter-chain behavior can be developed and validated as a standalone Goggles filter library while preserving existing runtime behavior.

## Problem

- `FilterChain` is exposed through backend APIs and called directly from app-layer wiring, so chain internals leak outside render boundaries.
- `src/render/chain` has reverse dependency on backend helper headers, which blocks clean library isolation.
- Shader/preset/texture responsibilities are interleaved with host backend ownership concerns, increasing regression risk for reload and resize paths.

## Scope

- Define a standalone Goggles filter library boundary that owns filter-chain, shader, and texture internals.
- Keep host backend responsibilities focused on swapchain, import, synchronization, and present.
- Remove direct exposure of concrete `FilterChain*` from backend public APIs and app-facing code by introducing backend-facing filter-chain boundary access.
- Remove reverse include coupling from chain to backend helper headers.

## Non-goals

- No change to user-facing shader semantics (`Source`, `OriginalHistory#`, `PassOutput#`, `PassFeedback#`, `<alias>Feedback`).
- No functional redesign of pre-chain/effect/post-chain behavior.
- No migration to non-Vulkan rendering APIs in this change.

## What Changes

- Introduce the `goggles-filter-chain` library boundary for filter runtime code.
- Re-scope ownership so chain+shader+texture internals are grouped under the filter library boundary, including shader-runtime ownership/creation.
- Add backend-facing filter-chain boundary methods for filter controls and parameter operations, replacing any external use of concrete `FilterChain*` and deprecating backend chain accessors that leak concrete internals.
- Add a boundary-safe `VulkanContext` contract placement decision and migration task so host<->filter initialization uses a boundary-owned header with boundary-allowed includes only.
- Add a boundary-safe control descriptor for both effect and prechain controls, with explicit stage domain (`prechain`, `effect`), deterministic enumeration order, stable `control_id` semantics, and optional `description` fallback behavior.
- Move control mutation/callback surfaces to `control_id` contracts (no `pass_index` leakage across backend/app/UI boundaries).
- Keep descriptor adapters behind the filter boundary so backend/app/UI do not depend on shader-internal metadata types.
- Document adapter mapping rules for effect and prechain controls, including asymmetries where one source lacks `current_value` at enumeration time.
- Remove backend helper header dependency from chain/shader/texture sources by introducing boundary-safe utility contracts and relocating boundary-safe `VK_TRY` usage.
- Add spec and task coverage for lifecycle (success and failure paths), stage-order, and semantic-binding parity invariants, plus stronger structural boundary guards.

## Capabilities

### New Capabilities
- `goggles-filter-chain`: Defines the standalone filter library boundary, ownership, public control surface, and fixed library name.

### Modified Capabilities
- `render-pipeline`: Updates render ownership boundaries so backend host duties and filter library duties are explicitly separated without changing output behavior.

## Risks

- Reload/resize regressions if lifecycle handoff between backend host and filter library is incomplete.
- Hidden dependency leaks if app/UI code still depends on concrete chain types.
- Policy-sensitive drift in error propagation or logging when moving boundaries.

## Validation Plan

- Verify unchanged rendering behavior for existing presets and semantic bindings.
- Verify async reload success and failure behavior, swapchain recreation, and resize paths preserve current behavior.
- Verify app/UI code paths operate through backend-facing filter-chain boundary methods only, with control callbacks keyed by `control_id`.
- Verify descriptor contracts: closed stage domain (`prechain`, `effect`), deterministic enumeration order, and out-of-range set-value handling.
- Verify chain sources no longer include backend helper headers.
- Verify boundary constraints: no app/UI includes of `render/chain/filter_chain.hpp`, no app/UI includes of `render/shader/*`, no app/UI/backend-public concrete-chain accessor usage, and no backend helper includes in boundary code.
- Verify one-way build dependency direction and dependency audit for `goggles-filter-chain` link targets.
- Keep CI/local preset test flows free of dedicated `check-filter-boundary.sh` guard gating; rely on tests plus source-audit checks for boundary regressions.
- Run existing render and integration test suites relevant to parser/preprocessor/shader runtime/filter-chain logic.

## Acceptance Criteria

- No backend public API, app code, or UI code exposes concrete chain types or accessors.
- Semantic parity explicitly includes `Source`, `OriginalHistory#`, `PassOutput#`, `PassFeedback#`, and `<alias>Feedback`.
- Descriptor contract is deterministic and testable: known stage domain values, deterministic order, and `control_id`-based callbacks.
- Async reload success and failure semantics are both specified and tested, and swap-signal semantics remain one-shot.
- Boundary leak constraints are maintained without dedicated guard-script CI gating.
- `VulkanContext` placement is explicit, boundary-safe, and covered by include/dependency checks.

## Impact

- Impacted modules: `src/render/chain`, `src/render/shader`, `src/render/texture`, `src/render/backend`, `src/app`, and related render tests.
- Impacted specs: new `goggles-filter-chain`; modified `render-pipeline`.
- Policy-sensitive areas touched:
  - error handling and propagation (`Result<T>`/`GOGGLES_TRY` consistency)
  - logging boundaries (avoid duplicate or suppressed logs)
  - threading/reload behavior (`util::JobSystem` reload path)
  - Vulkan API boundary split and resource ownership/lifetime semantics
