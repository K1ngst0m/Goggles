# Change: Add Nearest-Neighbor Prechain Downsampling

## Problem

The prechain DownsamplePass currently supports only area and gaussian filtering, so users cannot choose nearest-neighbor sampling when they want the sharpest possible downsampled output.

## Why

Nearest-neighbor is a standard filter option for image scaling and shader workflows, especially when users want crisp pixel edges instead of area-averaged or gaussian-smoothed results. Adding it now completes the existing prechain filter selector without introducing a parallel control path.

## Scope

- Extend the existing prechain `filter_type` selector to include a nearest-neighbor mode.
- Preserve current area and gaussian behavior exactly, including existing persisted values.
- Keep the change inside the existing prechain/downsample architecture and runtime control flow.
- Update the render-pipeline OpenSpec contract and verification tasks for the new mode.

## Non-goals

- Adding a separate prechain control or alternate sampling subsystem.
- Changing prechain/effect/postchain ordering or shader semantic contracts.
- Remapping existing area or gaussian values, or changing the default filter behavior.
- Broad UI redesign, preset-format expansion beyond the new mode, or unrelated filter-chain refactors.

## What Changes

### Runtime filter-mode expansion

- Extend `DownsamplePass` filter selection from two modes to three modes using the existing `filter_type` parameter.
- Add nearest-neighbor sampling behavior to the internal downsampling shader path.
- Keep runtime switching pipeline-free so control changes apply to subsequent frames without rebuild.

### Control and persistence compatibility

- Preserve the current prechain control surface and descriptor contract for `filter_type`.
- Keep existing area/gaussian persisted values stable and introduce nearest-neighbor as a new opt-in value.
- Ensure control snapshots and runtime parameter surfaces continue to expose deterministic prechain-first ordering.

### Contract updates

- Modify `render-pipeline` requirements for filter-type selection and downsample-pass behavior to cover nearest-neighbor semantics and the expanded discrete range.
- Keep implementation readiness explicit around automated verification for runtime behavior, compatibility, and prechain control coverage.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `render-pipeline`: extend downsample filter selection and prechain downsample behavior to support nearest-neighbor as a backward-compatible runtime mode.

## Impact

- **Affected specs:** `render-pipeline`
- **Affected code (expected):**
  - `src/render/chain/downsample_pass.cpp`
  - `src/render/chain/downsample_pass.hpp`
  - `src/render/chain/filter_chain.cpp`
  - `shaders/internal/downsample.frag.slang`
  - `tests/render/test_filter_controls.cpp`
  - targeted render/filter-chain tests for parameter parsing or snapshot coverage

## Policy-sensitive impacts

- **Error handling:** invalid or unsupported filter values MUST continue to be clamped or rejected through existing control contracts without silent behavior drift.
- **Logging:** no new cascading logs for ordinary runtime filter switching.
- **Threading:** runtime mode changes stay inside the existing single-threaded render/filter path; no new ad hoc threads.
- **Vulkan/runtime ownership:** nearest-neighbor support MUST reuse existing DownsamplePass ownership, descriptor, and pipeline lifetimes.

## Risks

| Risk | Severity | Likelihood | Mitigation |
|------|----------|------------|------------|
| Existing persisted values change meaning | HIGH | LOW | Keep area = 0 and gaussian = 1 semantics unchanged and add nearest-neighbor as a new explicit value |
| Control metadata or UI handling assumes max = 1 | MEDIUM | MEDIUM | Update control descriptor expectations and cover prechain control enumeration in tests |
| Shader/runtime branch adds aliasing or bypasses same-resolution passthrough behavior unexpectedly | MEDIUM | LOW | Extend spec scenarios and targeted verification for nearest-neighbor plus identity passthrough |
| Boundary/API consumers regress because control snapshots no longer behave deterministically | MEDIUM | LOW | Keep existing prechain control ordering contract and verify snapshot/control coverage |

## Verification Contract

- **Baseline gates:**
  - `pixi run build -p debug`
  - `pixi run build -p asan`
  - `pixi run build -p quality`
- **Environment-agnostic automated checks:**
  - `ctest --preset test -R "goggles_unit_tests" --output-on-failure`
  - `pixi run test -p asan`
- **Environment-sensitive checks:**
  - None expected for this proposal; nearest-neighbor support is confined to render/filter-chain logic and shader behavior covered by automated checks.
- **Manual fallback:**
  - Not allowed for the baseline build/static-analysis gates or ASAN test pass.
- **Mandatory checks with no fallback:**
  - `pixi run build -p quality`
  - `pixi run test -p asan`
- **Pass criteria:**
  - Build gates complete without warnings-as-errors or preset failures.
  - Targeted render/filter-chain tests confirm nearest-neighbor is exposed, selectable, and backward-compatible with area/gaussian semantics.
