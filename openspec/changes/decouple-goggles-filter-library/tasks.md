## 1. Boundary-safe Utility Contracts

- [x] 1.1 Introduce a boundary-safe Vulkan result helper contract (including `VK_TRY`) for filter-boundary code with boundary-allowed includes only.
- [x] 1.2 Remove dead backend-helper includes from chain/shader/texture boundary sources.
- [x] 1.3 Repoint remaining boundary call sites to the boundary-safe helper contract and enforce that backend helper headers are not included from boundary code.

## 2. Ownership and Descriptor Contract

- [x] 2.1 Define and implement boundary-safe `VulkanContext` placement for host<->filter initialization (boundary-owned header, backend-safe includes only).
- [x] 2.2 Migrate shader-runtime ownership/creation from backend-owned paths into the `goggles-filter-chain` boundary (depends on 1.1-1.3).
- [x] 2.3 Wire `src/render/shader` and `src/render/texture` targets under `goggles-filter-chain` ownership and remove backend ownership of those internals (depends on 2.2).
- [x] 2.4 Define a boundary-safe control descriptor contract for both effect and prechain controls with explicit stage domain (`prechain`, `effect`), deterministic enumeration ordering, `control_id` uniqueness/stability, and out-of-range set-value behavior.
- [x] 2.5 Implement descriptor adapters behind `goggles-filter-chain`, including explicit effect/prechain mapping rules for metadata asymmetries (for example missing `current_value` source fields) and adapter-focused tests.

## 3. Facade and Consumer Migration

- [x] 3.1 Introduce backend-facing facade operations for lifecycle/preset, frame submission, controls, and prechain/policy access (depends on 2.3-2.5).
- [x] 3.2 Enforce facade invariant: each invocation resolves the active chain/runtime at call time and MUST NOT cache `FilterChain*` across calls.
- [x] 3.3 Remove/deprecate backend chain accessor surfaces that expose concrete internals (for example `VulkanBackend::filter_chain()`) from backend public headers, then migrate all consumers to facade APIs.
- [x] 3.4 Migrate app-layer filter call paths to facade-only usage and remove direct concrete-chain access in `src/app/application.cpp`.
- [x] 3.5 Migrate UI types to curated control descriptors and remove direct shader/preprocessor metadata dependencies in `src/ui/imgui_layer.hpp` and `src/ui/imgui_layer.cpp`.
- [x] 3.6 Replace pass-index-based callback contracts with `control_id`-based callback contracts across UI/app/backend wiring.

## 4. Async Lifecycle and Build Boundary

- [x] 4.1 Preserve async reload/swap notification ordering and deferred-destroy safety after facade migration (depends on 3.1-3.6).
- [x] 4.2 Define and implement async reload failure semantics: if activation fails, old chain/runtime remains active and swap-complete signal remains unset.
- [x] 4.3 Keep chain-swapped signal semantics one-shot in success and failure paths (consumption clears success signal; failure emits no completion signal).
- [x] 4.4 Align build target naming to `goggles-filter-chain` and enforce one-way dependency direction: backend links `goggles-filter-chain`, but `goggles-filter-chain` does not link backend targets.
- [x] 4.5 Audit `goggles-filter-chain` link dependencies, explicitly preventing unnecessary SDL3/app/UI-only linkage when not required by chain/shader/texture runtime.
- [x] 4.6 Document and enforce deferred-destroy dual-ownership semantics (active runtime owned by filter boundary; retired runtime retained by host only until GPU-drain-safe destruction).

## 5. Structural Boundary Verification

- [x] 5.1 Enforce boundary constraints in implementation: no `render/chain/filter_chain.hpp` includes from `src/app`/`src/ui` and no concrete chain accessor exposure in backend public headers.
- [x] 5.2 Enforce include isolation in implementation: no `render/shader/*` includes from non-boundary consumers and no backend helper includes from chain/shader/texture boundary code.
- [x] 5.3 Remove `check-filter-boundary.sh`-based guard gating from local test flow and CI per project-direction update.
- [x] 5.4 Keep boundary regression coverage through render/boundary tests and review-time source audits rather than dedicated guard scripts.
- [x] 5.5 Record the guard-strategy change in proposal/design/spec artifacts to keep OpenSpec in sync with implemented workflow.
- [x] 5.6 Add or extend tests for stage-order parity (`pre-chain -> effect -> post-chain`) and semantic-binding parity (`Source`, `OriginalHistory#`, `PassOutput#`, `PassFeedback#`, `<alias>Feedback`).
- [x] 5.7 Add boundary API integration coverage for control enumeration/set/reset through facade APIs only, including stage-domain validation, deterministic ordering, `control_id` callbacks, and out-of-range set behavior.
- [x] 5.8 Add targeted integration coverage for async reload success/failure/resize behavior, including one-shot swap-signal correctness and deferred-destroy safety expectations.
- [x] 5.9 Keep OpenSpec deltas synchronized with implementation outcomes for `specs/goggles-filter-chain/spec.md` and `specs/render-pipeline/spec.md`, then mark completed tasks in this checklist.
- [x] 5.10 Run render-related unit coverage using presets: `ctest --preset test -R "goggles_unit_tests" --output-on-failure`.
- [x] 5.11 Run CI-parity validation for final changes: `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality`.
