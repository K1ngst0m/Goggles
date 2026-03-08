## Why

This change refactors a `VulkanBackend` implementation that had concentrated Vulkan bring-up,
output-target lifetime, DMA-BUF import, explicit sync, filter-chain hosting, and async preset
reload inside one public facade. That shape raised the risk of ownership drift, shutdown mistakes,
and behavior regressions whenever the render backend changed.

The resulting responsibility-oriented split keeps the existing public API and runtime behavior stable
while making lifecycle authority, dependency direction, and verification scope explicit enough for a
fresh `/goggles-apply` run.

## Problem

- The pre-refactor `src/render/backend/vulkan_backend.cpp` mixed four distinct backend behaviors
  behind one stateful implementation surface.
- Backend ownership and sequencing rules were spread across windowed, headless, DMA-BUF, and
  filter-chain paths, making low-risk edits harder.
- Async preset reload, explicit sync import, swapchain recreation, and headless readback all depend
  on auditable shutdown and frame-boundary ordering that needed an explicit refactor contract.
- Future backend work still needs a stable extraction order and verification contract so behavior
  preservation does not depend on undocumented repository context.

## Scope

- Keep `src/render/backend/vulkan_backend.hpp` as the public `VulkanBackend` facade.
- Split internal backend responsibilities into four concrete subsystems:
  - `VulkanContext`
  - `RenderOutput`
  - `ExternalFrameImporter`
  - `FilterChainController`
- Make ownership and coordination boundaries explicit for windowed, headless, filter-chain, and
  DMA-BUF paths.
- Define an implementation order and verification contract that preserves existing behavior during
  extraction.

## Non-goals

- Changing the public `VulkanBackend` API or `Application` call sites.
- Introducing a generic renderer framework, render graph, or cross-API abstraction layer.
- Splitting the backend into separate public windowed and headless backend classes.
- Changing DMA-BUF explicit sync semantics, filter stage ordering, or async preset reload policy.
- Changing the existing `goggles-filter-chain` ownership boundary beyond the living-spec contracts it
  already owns.

## What Changes

- The change defines a backend module-layout contract that preserves `VulkanBackend` as the public
  facade while requiring responsibility-oriented internal seams.
- The subsystem authority rules keep `VulkanBackend` as the cross-subsystem coordinator, keep each
  subsystem responsible for its own resources, and keep non-context subsystems from depending on one
  another.
- Long-lived filter runtime ownership, shader runtime ownership/creation, shader processing, and
  preset texture loading remain inside `goggles-filter-chain`, with backend-side filter coordination
  limited to boundary-facing sequencing, reload requests, policy inputs, and temporary GPU-drain-safe
  retirement.
- A boundary-owned host<->filter `VulkanContext` contract keeps filter-boundary code independent of
  backend-only context headers.
- The extraction order starts with declaration seams and proceeds through `VulkanContext`,
  `RenderOutput`, `ExternalFrameImporter`, `FilterChainController`, then final facade cleanup.
- Behavior-preservation requirements cover headless/windowed output, DMA-BUF import and explicit
  sync, filter-chain stage and async-reload behavior, and shutdown ordering.
- The verification contract combines preset-driven build/test gates with targeted backend checks for
  the highest-risk behavior seams.

## Capabilities

### New Capabilities
- `vulkan-backend-module-layout`: Internal backend refactor contract for preserving the
  `VulkanBackend` facade while extracting behavior-oriented subsystems with explicit ownership,
  sequencing, and verification rules.

### Modified Capabilities
- None.

## Risks

- `RenderOutput` could become a new mixed-responsibility owner if swapchain, headless, pacing, and
  rebuild policy are not kept behind one bounded seam.
- `VulkanBackend::render()` could remain a god method if frame-boundary contracts are not explicit.
- Extraction could duplicate authoritative state across facade and subsystem owners, especially for
  target extent/format, imported image lifetime, and pending filter runtime state.
- Verification could miss a behavior regression if DMA-BUF import, async swap, or headless paths do
  not keep dedicated checks.

## Validation Plan

Verification contract:
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
  - record runtime prerequisites, observations, and proof location for the manual run
- Mandatory checks with no fallback:
  - `pixi run build -p quality`
  - `pixi run test -p asan`
  - `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`
  - `build/test/tests/goggles_tests "[vulkan-backend-module-layout]"`
  - `build/test/tests/goggles_tests "[vulkan-backend-lifetime]"`
  - `ctest --preset test -R "^goggles_headless_integration$" --output-on-failure`
- Pass criteria:
  - preset build and test commands exit successfully
  - named backend module-layout and lifetime tests exit successfully
  - targeted grep output continues to show the expected backend entrypoints and proposal traceability
  - grep output shows no new expected-failure exception paths in backend scope
  - async preset reload remains traceable to `goggles::util::JobSystem`-backed behavior
  - forbidden direct include edges among `RenderOutput`, `ExternalFrameImporter`, and
    `FilterChainController` are absent
  - filter-boundary code continues to use a boundary-owned `VulkanContext` contract instead of
    backend-only context headers
  - `goggles_headless_integration` preserves DMA-BUF import and explicit sync behavior
  - `headless_smoke` preserves headless readback behavior when that environment-sensitive check is available

## Divergence Handling

- If implementation needs to change the `VulkanBackend` public API, `Application` call sites,
  subsystem dependency direction, DMA-BUF explicit sync semantics, headless behavior, filter
  stage-order semantics, or shutdown/async lifetime ordering, `/goggles-apply` MUST stop and
  reconcile proposal, design, specs, and tasks before implementation continues.
- If extraction reveals hidden coupling, the default remediation is to add narrow internal seams or
  adapters that preserve the declared subsystem boundaries without changing external behavior.

## Impact

- **Code modules/files**: `src/render/backend/vulkan_backend.hpp`,
  `src/render/backend/vulkan_backend.cpp`, new internal backend subsystem files under
  `src/render/backend/`, `src/render/backend/CMakeLists.txt`, and backend-touching tests under
  `tests/render/`.
- **Runtime behavior guarded**: windowed swapchain flow, headless offscreen flow,
  DMA-BUF/imported-image lifecycle, explicit sync submission wiring, async filter reload, and
  `Application` integration points.
- **Existing specs relied on but not modified**: `openspec/specs/render-pipeline/spec.md`,
  `openspec/specs/headless-mode/spec.md`, `openspec/specs/goggles-filter-chain/spec.md`, and
  `openspec/specs/object-lifecycle/spec.md`.
- **Vulkan ownership authority for this change**: `docs/project_policies.md` governs Vulkan lifetime
  and ownership rules for this refactor. The stale `vk::Unique*` wording in
  `openspec/specs/render-pipeline/spec.md` is excluded from this change contract until it is
  reconciled with policy before apply depends on it.
- **OpenSpec artifacts introduced by this change**:
  - `openspec/changes/vulkan-backend-refactor-subsystems/specs/vulkan-backend-module-layout/spec.md`
  - future living-spec sync target: `openspec/specs/vulkan-backend-module-layout/spec.md`
- **Policy-sensitive areas**:
  - Vulkan ownership and destruction ordering MUST stay explicit.
  - Expected runtime failures MUST remain `Result`-based.
  - Render-path async work MUST remain on `util::JobSystem`.
  - DMA-BUF explicit sync behavior MUST remain intact.
