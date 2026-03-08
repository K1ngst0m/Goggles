## 1. Declaration Seams and Buildability

- [x] 1.1 Add the narrow internal headers/types needed for a compile-safe backend split under `src/render/backend/` without changing the public `VulkanBackend` API in `src/render/backend/vulkan_backend.hpp`.
- [x] 1.2 Update `src/render/backend/CMakeLists.txt` as new backend translation units land so each extraction phase remains buildable.
- [x] 1.3 Add or update focused backend tests and scaffolding in `tests/render/` only where needed to keep the extraction verifiable by phase.
- [x] 1.4 Add or update focused audit coverage in `tests/render/` tagged for `"[vulkan-backend-module-layout]"` and `"[vulkan-backend-lifetime]"` so authority boundaries, forbidden dependency edges, and teardown ordering have named verification surfaces.

## 2. Extract `VulkanContext`

- [x] 2.1 Move instance, debug-messenger, optional surface, physical-device selection, logical-device creation, queue selection, and stable capability facts into `VulkanContext`.
- [x] 2.2 Keep `VulkanContext` as a stable root owner with `vk::` handles, explicit destruction, and `Result`-based factory/error flow.
- [x] 2.3 Preserve existing validation-layer behavior, headless device-selection behavior, and app-side Vulkan policy constraints during the move.
- [x] 2.4 Preserve or introduce a boundary-owned host<->filter `VulkanContext` contract or adapter so `goggles-filter-chain` does not consume backend-only context headers.

## 3. Extract `RenderOutput`

- [x] 3.1 Move swapchain target lifetime, frame-in-flight state, acquire/submit/present, pacing, and resize bookkeeping behind `RenderOutput`.
- [x] 3.2 Move headless offscreen target allocation, headless submission, and `readback_to_png` support behind the same output seam without creating a second public backend class.
- [x] 3.3 Keep target extent/format and frame-retirement authority in `RenderOutput`, and keep rebuild coordination triggered only by `VulkanBackend`.

## 4. Extract `ExternalFrameImporter`

- [x] 4.1 Move DMA-BUF plane-layout import, imported-image lifetime, and imported view/extent ownership into `ExternalFrameImporter`.
- [x] 4.2 Move temporary explicit-sync wait-object creation and retirement into `ExternalFrameImporter` so submit paths do not duplicate that ownership.
- [x] 4.3 Preserve existing DMA-BUF explicit sync semantics from compositor handoff through frame submission.

## 5. Extract `FilterChainController`

- [x] 5.1 Move backend-side reload request state, preset path state, policy inputs, boundary-facing control orchestration, and temporary GPU-drain-safe retirement bookkeeping into `FilterChainController` while leaving long-lived runtime ownership in `goggles-filter-chain`.
- [x] 5.2 Keep `goggles::util::JobSystem` as the only async rebuild request mechanism and preserve staged frame-boundary swap behavior through boundary-facing filter contracts.
- [x] 5.3 Keep filter controls, stage policy inputs, and prechain-resolution coordination in `FilterChainController` without moving shader runtime ownership/creation, preset texture loading, or output-policy decisions out of their existing owners.

## 6. Shrink `VulkanBackend` to Facade Coordination

- [x] 6.1 Rewrite `create()`, `create_headless()`, `render()`, `recreate_swapchain()`, `readback_to_png()`, and `shutdown()` to compose the four subsystem owners while keeping public behavior unchanged.
- [x] 6.2 Remove mirrored cached state from `VulkanBackend` when an authoritative subsystem owner already exists.
- [x] 6.3 Preserve `Application` integration points and keep cross-subsystem sequencing owned by `VulkanBackend` rather than by direct subsystem-to-subsystem calls.

## 7. Verification Contract

- [x] 7.1 Run baseline gates:
  - `pixi run build -p debug`
  - `pixi run build -p asan`
  - `pixi run build -p quality`
- [x] 7.2 Run environment-agnostic automated checks:
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
- [x] 7.3 Run environment-sensitive checks when local runtime support exists:
  - `ctest --preset test -R "^headless_smoke$" --output-on-failure`
- [x] 7.4 Allow manual fallback only for `headless_smoke`; record prerequisites, observations, and proof location if that fallback is used.
- [x] 7.5 Treat these checks as mandatory with no fallback:
  - `pixi run build -p quality`
  - `pixi run test -p asan`
  - `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`
  - `build/test/tests/goggles_tests "[vulkan-backend-module-layout]"`
  - `build/test/tests/goggles_tests "[vulkan-backend-lifetime]"`
  - `ctest --preset test -R "^goggles_headless_integration$" --output-on-failure`

## 8. Spec and Divergence Control

- [x] 8.1 Keep implementation aligned with `openspec/changes/vulkan-backend-refactor-subsystems/specs/vulkan-backend-module-layout/spec.md`, `design.md`, and the referenced living specs for render-pipeline, headless-mode, filter-chain, and object-lifecycle behavior.
- [x] 8.2 If implementation requires public API drift, dependency-direction drift, DMA-BUF explicit-sync drift, headless/windowed behavior drift, filter stage-order drift, or shutdown/async lifetime ordering drift, stop apply work and reconcile proposal, design, specs, and tasks before continuing.
- [x] 8.3 Record phase-by-phase verification evidence so each extracted seam has an auditable build/test result before the next phase starts.
- [x] 8.4 Reconcile the stale `vk::Unique*` ownership wording in `openspec/specs/render-pipeline/spec.md` or carry an explicit spec delta before implementation relies on that living-spec language for backend ownership semantics.

## 9. Requirement Traceability

- [x] 9.1 Keep this mapping updated during apply so each requirement and major preserved behavior has at least one implementation task and one verification command.

| Requirement (spec.md) | Task IDs | Verification commands |
| --- | --- | --- |
| VulkanBackend Facade Remains Stable | 1.1, 6.1, 6.3 | `pixi run build -p debug`; `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` |
| Behavior-Oriented Backend Subsystems | 2.1, 3.1, 4.1, 5.1 | `pixi run build -p debug`; `build/test/tests/goggles_tests "[vulkan-backend-module-layout]"` |
| Filter Boundary Ownership Remains Intact | 2.4, 5.1, 5.3, 8.1 | `build/test/tests/goggles_tests "[vulkan-backend-module-layout]"`; `grep -R --line-number '#include "vulkan_context.hpp"' src/render/chain src/render/shader src/render/texture --include="*.cpp" --include="*.hpp"` |
| Subsystem Authority and Dependency Direction Remain Explicit | 3.3, 4.2, 5.3, 6.2, 6.3, 7.2 | `build/test/tests/goggles_tests "[vulkan-backend-module-layout]"`; `grep -R --line-number '#include "external_frame_importer.hpp"\|#include "filter_chain_controller.hpp"' src/render/backend/render_output.*`; `grep -R --line-number '#include "render_output.hpp"\|#include "filter_chain_controller.hpp"' src/render/backend/external_frame_importer.*`; `grep -R --line-number '#include "render_output.hpp"\|#include "external_frame_importer.hpp"' src/render/backend/filter_chain_controller.*` |
| Boundary-Owned VulkanContext Contract Remains Intact | 2.4, 5.1, 7.2, 8.1 | `build/test/tests/goggles_tests "[vulkan-backend-module-layout]"`; `grep -R --line-number '#include "vulkan_context.hpp"' src/render/chain src/render/shader src/render/texture --include="*.cpp" --include="*.hpp"` |
| Extraction Contract Is Explicit for Apply | 1.2, 7.1, 7.2, 8.3 | `pixi run build -p debug`; `pixi run test -p asan` |
| Backend Behavior Is Preserved Across the Split | 3.2, 4.3, 5.2, 6.1, 7.2, 7.3 | `pixi run test -p asan`; `ctest --preset test -R "^(headless_smoke|goggles_headless_integration)$" --output-on-failure` |
| Shutdown and Async Lifetime Ordering Remain Safe | 1.4, 2.2, 5.1, 6.1, 7.2, 7.5, 8.2 | `build/test/tests/goggles_tests "[vulkan-backend-lifetime]"`; `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` |
| Preserved Result-Based Error Flow | 2.2, 6.1, 7.2, 8.1 | `pixi run build -p quality`; `grep -R --line-number "\bthrow\b" src/render/backend tests/render --include="*.cpp" --include="*.hpp"` |
| Preserved `goggles::util::JobSystem` Async Reload Behavior | 5.1, 5.2, 6.1, 7.2 | `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`; `grep -R --line-number "util::JobSystem\|reload_shader_preset" src/render/backend tests/render --include="*.cpp" --include="*.hpp"` |
