## Context

The current render path places filter-chain orchestration inside `src/render/chain`, but backend lifecycle and app/UI control flows still directly depend on concrete chain types. This increases coupling across `src/render/backend`, `src/app`, and `src/ui` and makes isolated testing and evolution harder.

Project constraints that shape this design:
- fallible operations MUST remain `Result`-based and policy-compliant
- Vulkan result handling MUST stay explicit
- render/pipeline async work MUST remain on `util::JobSystem`
- behavior contracts for shader semantics and stage ordering MUST remain unchanged

## Goals / Non-Goals

**Goals:**
- Establish a standalone Goggles filter library boundary owning chain + shader + texture internals.
- Keep host backend responsibilities focused on swapchain/import/synchronization/present.
- Remove backend-public/app/UI exposure of concrete `FilterChain*` in favor of backend-facing facade methods.
- Remove reverse dependency from chain sources to backend helper headers.
- Name the new library `goggles-filter-chain`.

**Non-Goals:**
- Changing filter output behavior or RetroArch semantic contracts.
- Replacing Vulkan backend host responsibilities.
- Multi-API runtime expansion in this change.

## Decisions

1. **Boundary extraction at filter-runtime level**
   - Decision: The extracted boundary includes chain, shader, and texture internals together, including shader-runtime ownership and creation.
   - Rationale: These components are currently co-dependent in preset load and pass materialization paths.
   - Alternatives considered:
     - Split only `chain/` and keep shader/texture outside: rejected due continued tight runtime coupling.
     - Extract full `render/` stack: rejected as too broad for a safe incremental change.

2. **Backend remains host/runtime owner for present path**
   - Decision: Host backend keeps swapchain, external image import, queue submission, and present control.
   - Rationale: These responsibilities are tied to window/surface lifecycle and platform integration.
   - Alternatives considered:
     - Move present/import logic into filter library: rejected due boundary blur and increased host integration risk.

3. **Facade-based access instead of concrete chain pointer exposure**
   - Decision: App/UI-facing code uses backend facade operations, not concrete `FilterChain*`.
   - Rationale: Reduces cross-layer dependency leakage and supports isolated testing.
   - Alternatives considered:
     - Keep pointer exposure and document usage constraints: rejected as unenforceable and brittle.

4. **Dependency direction cleanup in chain sources**
   - Decision: Chain/shader/texture sources stop including backend helper headers and rely on boundary-safe utilities/contracts.
   - Rationale: A standalone library cannot depend on host backend internals.
   - Concrete boundary-safe migration steps:
     - remove dead backend-helper includes where they are not needed
     - relocate `VK_TRY` into a boundary-safe helper header with boundary-allowed dependencies only
     - repoint remaining call sites to the boundary-safe header and enforce include guards
   - Alternatives considered:
      - Keep includes and duplicate backend helpers: rejected for circular dependency and maintenance cost.

5. **Library name**
   - Decision: The extracted library is named `goggles-filter-chain`.
   - Rationale: Fixed project naming decision for this change.

6. **Boundary control model**
    - Decision: The boundary exposes curated control structs only; pass-level parameter metadata is not part of the stable boundary.
    - Rationale: Keeps app/UI and downstream tests decoupled from pass internals while preserving behavior-level control.
    - Boundary descriptor contract:
      - stable fields include `control_id`, `stage`, `name`, optional `description`, `current_value`, `default_value`, `min_value`, `max_value`, and `step`
      - descriptor contract covers effect-stage and prechain controls with a closed stage domain (`prechain`, `effect`)
      - descriptor enumeration order is deterministic and stable for equivalent reloads
      - set-value requests outside descriptor bounds are clamped to `[min_value, max_value]`
      - UI fallback: when `description` is absent, UI uses `name` without tooltip text
      - control mutation/callback surfaces use `control_id` and do not expose pass indices
    - `control_id` semantics:
      - MUST be unique within an active preset
      - MUST remain stable across reloads of the same preset when control layout is unchanged
      - MAY change when switching to a different preset with a different control layout
      - pass indices, reflection internals, and descriptor-binding internals are excluded
   - Alternatives considered:
      - Expose pass-level metadata directly: rejected due direct leakage of internal pass graph details.

7. **Stable downstream test surface**
   - Decision: Downstream tests rely on a minimal facade API only.
   - Stable API groups:
      - lifecycle and preset: create/shutdown, load/reload preset, current preset path, chain-swapped signal
      - frame submission: record/filter-frame entry with caller-provided frame context and stage policy
      - controls: list controls, set control value by `control_id`, reset controls
      - prechain and policy: set/get prechain resolution, set stage policy, prechain-control operations
      - control descriptors: backend-safe descriptor list used by UI and downstream tests
   - Not stable:
      - concrete `FilterChain*` exposure and pass-level concrete types
      - pass indices and internal pass ordering details within a stage
      - internal async swap/deferred-destroy mechanics
      - UI contracts tied to shader-internal metadata structs

8. **Adapter ownership location**
    - Decision: descriptor adapters from shader/chain internals to curated control descriptors live behind the `goggles-filter-chain` boundary.
    - Rationale: Prevents backend/app/UI from depending on shader-internal metadata structs.
    - Mapping rules:
      - effect and prechain metadata sources must map to one descriptor schema
      - if a source omits `current_value`, adapter emits runtime-effective value (or `default_value` when no runtime override exists)

9. **VulkanContext boundary contract placement**
    - Decision: host<->filter initialization uses a boundary-owned `VulkanContext` contract header with boundary-allowed includes only.
    - Rationale: Prevents reverse dependency leaks where backend-only headers are dragged into filter-boundary APIs.
    - Alternatives considered:
      - Keep `VulkanContext` under backend headers: rejected due recurring boundary leakage risk.

10. **Facade invocation safety invariant**
   - Decision: facade methods MUST resolve the active chain reference at call time and MUST NOT cache `FilterChain*` across calls.
   - Rationale: Preserves async reload/swap safety and avoids stale-pointer use across deferred-destroy windows.

## Risks / Trade-offs

- [Lifecycle regressions on reload/resize] -> Keep async reload/swap flows behavior-equivalent and validate with existing render/integration tests.
- [Residual direct chain access from app/UI] -> Introduce explicit facade methods and remove downstream dependency paths.
- [Policy drift during boundary move] -> Keep `Result`/logging/threading contracts explicit in tasks and reviews.
- [Build graph churn] -> Stage target/link changes incrementally and keep current runtime behavior as acceptance gate.
- [Ownership confusion for retired runtime] -> Document active-vs-retired dual ownership and gate with lifecycle tests covering success and reload-failure paths.

## Migration Plan

1. Establish boundary-safe utility contracts and relocate boundary-safe `VK_TRY` usage.
2. Place `VulkanContext` in a boundary-safe contract header and migrate host<->filter initialization to that contract.
3. Define curated control descriptor contract (including closed stage domain and deterministic ordering) and implement adapters behind `goggles-filter-chain`.
4. Introduce/complete facade operations with call-time active-chain resolution.
5. Remove/deprecate backend concrete-chain accessor exposure and migrate app/UI paths to facade-only usage.
6. Migrate UI control plumbing to curated descriptors and `control_id`-keyed callbacks.
7. Preserve and verify async reload success/failure behavior, swap one-shot signaling, and deferred-destroy safety invariants.
8. Align build targets/naming and wire chain/shader/texture ownership under `goggles-filter-chain`.
9. Validate structural boundaries via tests and source-audit checks (including shader-header isolation) without dedicated guard-script CI/local gating; keep rollback to prior link wiring if regressions appear.

## Open Questions

- None.
