## Context

The starting point for this change had `src/render/backend/vulkan_backend.cpp` owning Vulkan
instance/device bring-up, swapchain and headless target lifetime, DMA-BUF import and explicit sync
ingress, filter-chain runtime ownership, async preset rebuild, and final render/present
orchestration behind one public `VulkanBackend` facade.

This refactor is brownfield and behavior-preserving. Existing runtime contracts remain anchored by:

- `docs/project_policies.md`
- `openspec/specs/render-pipeline/spec.md`
- `openspec/specs/headless-mode/spec.md`
- `openspec/specs/goggles-filter-chain/spec.md`
- `tests/render/test_filter_boundary_contracts.cpp`
- `docs/dmabuf_sharing.md`

`docs/project_policies.md` remains authoritative for Vulkan ownership and lifetime rules. The stale
`vk::Unique*` wording in `openspec/specs/render-pipeline/spec.md` is not part of this refactor
contract and MUST be reconciled before apply depends on that living-spec language for ownership
semantics.

The chosen design reduces edit surface without changing `Application` integration,
headless/windowed execution, DMA-BUF explicit sync semantics, or filter-chain stage policy and
async reload behavior.

## Goals / Non-Goals

**Goals:**

- Preserve `VulkanBackend` as the public integration facade.
- Extract internal seams that match real backend behaviors instead of resource buckets.
- Keep ownership, frame-boundary sequencing, and shutdown order auditable.
- Keep headless, windowed, DMA-BUF, and filter-chain behavior stable through a dependency-ordered
  migration plan.
- Make `/goggles-apply` deterministic from OpenSpec artifacts alone.

**Non-Goals:**

- Adding a new renderer abstraction or replacing Vulkan-specific contracts.
- Changing the public backend API, `Application` wiring, or filter-chain wrapper boundary.
- Reworking compositor protocols, capture-layer contracts, or shader semantics.
- Changing stage order, explicit sync rules, or moving render-path concurrency off
  `goggles::util::JobSystem`.

## Decisions

### 1) Keep `VulkanBackend` as a thin public facade

Decision:

- `VulkanBackend` remains the only public class integrated by app code.
- `vulkan_backend.hpp` remains the public declaration surface.
- The facade keeps public entrypoints, cross-subsystem orchestration, and destructive transitions
  such as startup composition, `render()`, `recreate_swapchain()`, `readback_to_png()`, and
  `shutdown()`.

Rationale:

- Preserves application call sites and minimizes externally visible change risk.
- Keeps cross-seam sequencing in one auditable place.

Alternatives considered:

- Separate public windowed/headless backend classes: rejected because it changes app integration and
  duplicates shared lifecycle rules.
- A public generic renderer interface: rejected as out of scope and contrary to concrete Vulkan
  contracts.

### 2) Extract four behavior-oriented subsystem seams

Decision:

- Use four internal subsystem owners:
  - `VulkanContext` for instance/device/queue/surface bring-up and stable capability facts
  - `RenderOutput` for windowed/headless render targets and frame retirement
  - `ExternalFrameImporter` for imported image lifetime and temporary wait-semaphore ownership
  - `FilterChainController` for backend-side boundary-facing filter coordination, reload requests,
    policy inputs, current preset path state, and temporary GPU-drain-safe retirement

Rationale:

- These seams already exist in the current code as distinct behaviors with different ownership and
  sequencing rules.
- The split keeps subsystem contracts concrete and Vulkan-specific.

Alternatives considered:

- Resource-bucket split (device resources vs frame resources vs filter resources): rejected because
  it preserves coupling across behavior boundaries.
- Expanding backend ownership across the existing filter boundary: rejected because living specs keep
  long-lived filter runtime ownership, shader runtime ownership/creation, and preset texture loading
  inside `goggles-filter-chain`.

### 3) Enforce one-way dependency direction and single authority per mutable state domain

Decision:

- `VulkanBackend` coordinates cross-subsystem transitions.
- `VulkanContext` provides stable Vulkan handles/capability facts to the other subsystems.
- `RenderOutput`, `ExternalFrameImporter`, and `FilterChainController` MAY depend on
  `VulkanContext`, but they MUST NOT depend on each other.
- Long-lived filter runtime ownership remains in `goggles-filter-chain`; backend-side
  `FilterChainController` owns only boundary-facing sequencing state and temporary host-side
  retirement required for GPU-drain-safe teardown.
- Target extent/format authority lives in `RenderOutput`.
- Imported image lifetime and temporary GPU-wait ownership live in `ExternalFrameImporter`.
- Stage-policy input state, current preset path state, and temporary retirement bookkeeping live in
  `FilterChainController`.
- `VulkanBackend` MUST avoid mirrored cached state when an authoritative subsystem owner already
  exists.

Rationale:

- Prevents hidden back-calls and authority confusion during the split.
- Keeps shutdown and rebuild logic auditable.

Alternatives considered:

- Letting `RenderOutput` trigger filter rebuilds or `FilterChainController` infer output policy:
  rejected because it recreates cross-owner coupling.

### 4) Use frame-boundary subsystem contracts instead of many small mutators

Decision:

- `RenderOutput` exposes one bounded begin/submit lifecycle for per-frame output work plus one
  explicit rebuild path.
- `ExternalFrameImporter` exposes one import operation that returns the imported-source state needed
  for the current frame and owns retirement of replaced resources.
- `FilterChainController` exposes one frame-boundary coordination path for reload requests,
  boundary-facing runtime handoff, and host-side retirement bookkeeping plus explicit load/rebuild
  request entrypoints.
- `render()` remains the only place that sees the current frame target, imported source, and filter
  runtime together.

Rationale:

- Reduces state-machine leakage back into the facade.
- Preserves hot-path clarity and keeps mode-specific policy local to the owning subsystem.

Alternatives considered:

- Many narrow getters/setters on each subsystem: rejected because it preserves distributed mutable
  state and increases orchestration complexity.

### 5) Preserve the boundary-owned `VulkanContext` contract for host<->filter initialization

Decision:

- Backend-owned `VulkanContext` implementation state MUST be adapted into the boundary-owned
  host<->filter `VulkanContext` contract required by `goggles-filter-chain`.
- Filter-boundary code MUST NOT include or depend on backend-only context headers.

Rationale:

- Keeps dependency direction compatible with the existing filter-boundary living spec.
- Prevents backend extraction from pulling backend internals into the standalone filter boundary.

Alternatives considered:

- Sharing backend-only `VulkanContext` headers directly with the filter boundary: rejected by the
  boundary-safe contract requirements.

### 6) Extraction order remains dependency-first and compile-safe

Decision:

- Apply work starts with narrow internal headers/types and any required `src/render/backend/CMakeLists.txt`
  updates to keep intermediate states buildable.
- Extraction order is fixed:
  1. declaration seams
  2. `VulkanContext`
  3. `RenderOutput`
  4. `ExternalFrameImporter`
  5. `FilterChainController`
  6. final `VulkanBackend` cleanup

Rationale:

- Moves lower-risk, lower-coupling seams first.
- Defers the most behavior-sensitive runtime owner (`FilterChainController`) until the output and
  import seams are stable.

Alternatives considered:

- Extracting filter-chain control first: rejected because it depends on output format/extent and the
  current render orchestration shape.

### 7) Shutdown and verification contracts are part of the design, not apply-time improvisation

Decision:

- Shutdown order remains explicit: wait pending async rebuild, clear pending-ready state, idle the
  device, destroy filter runtimes, destroy imported image state, destroy output resources, then
  destroy context-owned device/surface/debug/instance state.
- Verification contract:
  - Baseline gates:
    - `pixi run build -p debug`
    - `pixi run build -p asan`
    - `pixi run build -p quality`
  - Environment-agnostic automated checks:
    - `pixi run test -p asan`
    - `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`
    - `build/test/tests/goggles_tests "[vulkan-backend-module-layout]"`
    - `build/test/tests/goggles_tests "[vulkan-backend-lifetime]"`
    - `grep -R --line-number "VulkanBackend::create_headless\|readback_to_png\|reload_shader_preset" src/render/backend tests/render openspec/changes/vulkan-backend-refactor-subsystems`
    - `grep -R --line-number "\bthrow\b" src/render/backend tests/render --include="*.cpp" --include="*.hpp"`
    - `grep -R --line-number "util::JobSystem\|reload_shader_preset" src/render/backend tests/render --include="*.cpp" --include="*.hpp"`
    - `grep -R --line-number '#include "external_frame_importer.hpp"\|#include "filter_chain_controller.hpp"' src/render/backend/render_output.*`
    - `grep -R --line-number '#include "render_output.hpp"\|#include "filter_chain_controller.hpp"' src/render/backend/external_frame_importer.*`
    - `grep -R --line-number '#include "render_output.hpp"\|#include "external_frame_importer.hpp"' src/render/backend/filter_chain_controller.*`
    - `grep -R --line-number '#include "vulkan_context.hpp"' src/render/chain src/render/shader src/render/texture --include="*.cpp" --include="*.hpp"`
  - Environment-sensitive checks:
    - `ctest --preset test -R "^headless_smoke$" --output-on-failure` when the local runtime supports headless Vulkan readback coverage
  - Manual fallback:
    - allowed only for `headless_smoke`
    - record prerequisites, observations, and proof location
  - Mandatory checks with no fallback:
    - `pixi run build -p quality`
    - `pixi run test -p asan`
    - `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`
    - `build/test/tests/goggles_tests "[vulkan-backend-module-layout]"`
    - `build/test/tests/goggles_tests "[vulkan-backend-lifetime]"`
    - `ctest --preset test -R "^goggles_headless_integration$" --output-on-failure`
  - Pass criteria:
    - all baseline and mandatory commands succeed
    - named backend module-layout and lifetime tests succeed
    - grep evidence shows the declared backend entrypoints still exist and remain traceable
    - grep evidence shows no new expected-failure exception paths in backend scope
    - async preset reload remains traceable to `goggles::util::JobSystem`
    - forbidden direct include edges among non-context backend subsystems are absent
    - filter-boundary code continues to consume a boundary-owned `VulkanContext` contract rather than backend-only context headers
    - environment-sensitive checks preserve headless readback and DMA-BUF/import behavior when the runtime supports them

Rationale:

- The highest-risk failure modes here are behavioral regressions during extraction, not API novelty.
- The change needs a deterministic contract for `/goggles-apply`.

Alternatives considered:

- Defining verification ad hoc during apply: rejected because it makes behavior preservation less
  auditable.

## Risks / Trade-offs

- [Risk] `RenderOutput` absorbs too much policy -> Mitigation: keep it limited to target lifetime,
  retirement, pacing, and one rebuild path.
- [Risk] `VulkanBackend::render()` stays too large -> Mitigation: confine it to coordination and use
  private facade helpers without reintroducing subsystem back-calls.
- [Risk] authority duplication survives the split -> Mitigation: explicitly name the single owner for
  target, imported-source, and runtime state in specs/tasks.
- [Risk] async reload or deferred-destroy behavior regresses -> Mitigation: keep those behaviors
  owned by `FilterChainController` and require explicit verification.
- [Trade-off] more files and internal headers increase local indirection -> Mitigation: keep the
  dependency graph acyclic and the extraction order narrow.

## Migration Plan

1. Add only the narrow internal declarations and build-graph changes needed to make multi-file
   extraction compile-safe.
2. Extract `VulkanContext` and move bring-up/state facts without altering runtime flow.
3. Extract `RenderOutput`, first stabilizing swapchain mode and then headless mode under the same
   seam.
4. Extract `ExternalFrameImporter` so imported image state and temporary wait-semaphore ownership are
   no longer duplicated across submit paths.
5. Preserve or introduce the boundary-owned host<->filter `VulkanContext` adapter before backend
   filter-coordination extraction depends on it.
6. Extract `FilterChainController` with existing async rebuild requests, staged swap handoff, and
   temporary host-side retirement semantics intact while leaving long-lived runtime ownership in
   `goggles-filter-chain`.
7. Add focused module-layout and lifetime audit tests/tags before final cleanup depends on them.
8. Shrink `VulkanBackend` to facade orchestration and remove mirrored state.
9. Run the verification contract and stop immediately if the work requires contract or behavior
   divergence.

Rollback strategy:

- Revert by phase boundary, preserving the last compile-safe state.
- Do not ship a partial extraction that changes public API or dependency direction.
- If a phase uncovers behavior divergence or shutdown/async lifetime ordering drift that cannot be
  resolved inside the declared contract, stop and update proposal/design/spec/tasks before resuming
  apply.
