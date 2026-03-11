# Proposal: Profiling GPU Timestamp and Debug Label Test Coverage

## Intent

The archived `filter-chain-diagnostics` change already shipped `GpuTimestampPool`, Vulkan debug labels, and GPU timing integration, but verification still fails because the profiling scenario `GPU timestamps unavailable` cannot be proven on timestamp-capable hardware with the current product/test contract. Existing tests now cover the available-timestamp path and debug-label behavior, yet the unavailable-path still depends on the active GPU environment.

This change now expands from pure test coverage into a small, deliberate testability improvement: add a deterministic seam that lets tests force the unavailable-timestamp path without trying to stub Vulkan-Hpp physical-device property queries. The goal remains spec evidence, not new profiling behavior.

## Scope

### In Scope

- Add a minimal production seam for timestamp capability injection or explicit unavailable-pool construction so tests can deterministically exercise the unavailable-timestamp branch on timestamp-capable hardware
- Keep the seam narrowly scoped to profiling capability determination and pool creation, without introducing a broader Vulkan abstraction layer
- Extend `GpuTimestampPool` tests to prove both available and forced-unavailable behavior without environment-dependent skips
- Extend Tier 1 runtime diagnostics coverage so the unavailable-timestamps diagnostic event is asserted deterministically
- Preserve the existing debug-label and GPU-duration runtime evidence already added for this change

### Out of Scope

- New profiling product features or behavior changes visible to end users
- Broad refactors of Vulkan capability discovery or general mocking of `vk::PhysicalDevice` queries
- Reworking the diagnostic event model, sink infrastructure, or execution timeline schema
- Tracy instrumentation tests or Tier 2 forensic capture coverage

## Approach

Introduce the smallest production-facing seam that can represent `timestamps unavailable` as a first-class testable condition. Preferred direction: make timestamp availability/pool construction injectable at the profiling boundary, or provide a constrained factory/helper that can deliberately construct an unavailable `GpuTimestampPool` for tests and runtime callers that need deterministic behavior. Avoid trying to intercept Vulkan-Hpp static property queries, which would widen scope and couple tests to brittle dispatch details.

Then update the profiling tests to use that seam in two places: isolated `GpuTimestampPool` coverage for forced-unavailable behavior, and Tier 1 runtime diagnostics coverage that proves the diagnostic info event emitted when Tier 1 requests GPU timing but timestamps are unavailable. Keep the rest of the runtime path unchanged.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/util/diagnostics/gpu_timestamp_pool.hpp` | Modified | Define the minimal testability seam for timestamp capability or unavailable-pool creation |
| `src/util/diagnostics/gpu_timestamp_pool.cpp` | Modified | Implement the constrained unavailable-path construction without changing normal runtime behavior |
| `src/render/chain/chain_executor.cpp` | Modified | Thread the seam through the runtime profiling path only if needed for deterministic Tier 1 unavailable coverage |
| `tests/render/test_gpu_timestamp_pool.cpp` | Modified | Replace environment-limited unavailable-path coverage with deterministic assertions |
| `tests/render/test_runtime_diagnostics.cpp` | Modified | Add deterministic Tier 1 unavailable-timestamp event coverage while preserving existing available-path/debug-label evidence |
| `tests/CMakeLists.txt` | Modified | Keep profiling test sources registered if test layout changes |

## Impacted OpenSpec Specs

| Spec | Impact |
|------|--------|
| `profiling` | No spec changes — the seam exists only to prove already-specified behavior deterministically |

## Policy-Sensitive Impacts

- **Vulkan API**: Keep using `vk::` APIs only; do not introduce raw `Vk*`, `vk::Unique*`, or `vk::raii::*` wrappers as part of the seam
- **Error handling**: Unavailable-timestamp construction must preserve existing `Result`/expected-style behavior and no-op safety guarantees
- **Architecture**: Limit the seam to profiling capability selection; avoid creating a repo-wide device-capabilities abstraction just for tests
- **Tests**: Prefer deterministic assertions over GPU-environment branching for the unavailable path; retain `SKIP()` only for truly hardware-dependent available-path evidence

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| The seam grows into a wider production abstraction than needed | Medium | Keep the contract narrowly focused on timestamp availability or unavailable-pool construction only |
| The new seam invalidates the earlier no-design-needed assumption | High | Treat this proposal update as a trigger for a focused design pass before more implementation |
| Runtime wiring changes could accidentally alter normal timestamp-capable behavior | Medium | Preserve the default production path and add tests that prove both default-available and forced-unavailable behavior |
| Existing tasks no longer accurately describe the next implementation slice | High | Refresh tasks after design confirms the exact seam shape |

## Rollback Plan

Remove the production seam and any related deterministic unavailable-path tests, then fall back to environment-gated coverage only. This reverts the change to its prior test-only scope.

## Dependencies

- Existing `GpuTimestampPool` implementation in `src/util/diagnostics/gpu_timestamp_pool.*`
- Existing Tier 1 profiling runtime tests in `tests/render/test_runtime_diagnostics.cpp`
- Existing profiling spec in `openspec/specs/profiling/spec.md`
- Prior verification report identifying the environment-limited unavailable-timestamp gap

## Success Criteria

- [ ] The proposal reflects the expanded scope from test-only coverage to a minimal production testability seam
- [ ] The chosen seam can deterministically represent `GPU timestamps unavailable` on timestamp-capable hardware
- [ ] `GpuTimestampPool` unavailable-path tests no longer rely on environment-limited skips for their core assertion
- [ ] Tier 1 runtime diagnostics can deterministically assert the unavailable-timestamps diagnostic event
- [ ] Normal timestamp-capable runtime behavior and existing debug-label/GPU-duration evidence remain intact
- [ ] No profiling spec delta is required because user-visible behavior is unchanged
- [ ] Follow-on design/tasks work can describe the implementation slice precisely enough to unblock apply and future verify
