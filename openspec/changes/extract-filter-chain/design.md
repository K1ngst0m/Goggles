# Design: Extract Filter Chain

## Technical Approach

This change remains the original four-phase extraction program from `openspec/changes/extract-filter-chain/proposal.md`: prepare the monorepo, create the standalone repository, switch goggles to the standalone repository via submodule, then complete validation and cleanup. The design is updated to converge the library boundary without changing that program.

The end-state boundary is:

- `goggles-filter-chain` owns the executable runtime, preset/program loading, shader compilation/reflection, embedded assets, control enumeration/mutation, record-time rendering, and bounded inspection that is meaningful to non-goggles consumers.
- Goggles owns host orchestration, end-to-end application integration, swapchain/import/presentation concerns, and host-specific visual verification workflows.
- Intermediate pass capture is not treated as a proven stable public API requirement. If it exists during migration, it is transitional or test-support surface that must be internalized or otherwise clearly marked non-stable before this change is complete.
- Public diagnostics, if retained, are narrowed to the minimum justified caller-facing surface. The stable shape is passive metadata/reporting, not a broad session-lifecycle-driven contract unless later evidence explicitly justifies that broader surface.

This keeps the original extraction goals intact while reconciling the diagnostics/test-boundary conclusions into the source-of-truth design.

## Architecture Decisions

### Decision: Preserve the original extraction program and phase structure

**Choice**: Keep the existing four phases and the standalone-repo goal exactly as proposed, but make the diagnostics/test-boundary cleanup an explicit completion requirement inside those phases.
**Alternatives considered**: Start a follow-up change for boundary cleanup; freeze the current broader API as-is.
**Rationale**: The extraction is still the right program. The design gap is not whether to extract, but how to converge the public boundary before declaring the original change complete.

### Decision: FC stays centered on runtime mechanism and bounded inspection

**Choice**: Treat lifecycle, record, reports, errors, control enumeration/query/mutation, semantic control lookup, source loading, and log routing as the intended stable caller-facing surface.
**Alternatives considered**: Expand the stable surface to include all currently exported diagnostics/capture affordances; reduce FC to a thinner internal engine with goggles-owned wrappers.
**Rationale**: The extracted library is meant to be reusable on its own. Stable API should cover reusable runtime behavior and bounded inspection, not host-specific debugging workflows.

### Decision: Diagnostics policy narrows to the minimum justified public surface

**Choice**: Keep only the minimum stable diagnostics contract justified by external-consumer needs. A public diagnostics summary, if retained, is passive metadata retrieved from chain state. Diagnostics mode is the only caller-facing policy knob presumed stable by this change unless stronger evidence appears during implementation.
**Alternatives considered**: Treat diagnostic session lifecycle as stable public API; expose broad diagnostics configuration and artifact plumbing as part of the extracted contract.
**Rationale**: Current evidence supports a narrow inspection surface, not a broad lifecycle-driven diagnostics subsystem. Passive summary metadata is easier to support across hosts and is less likely to encode goggles-specific testing assumptions.

### Decision: Intermediate pass capture is not part of the stable library boundary

**Choice**: Do not define intermediate pass capture as a required stable public API outcome of this change. If pass capture remains needed during migration, implement it as internal/test-support plumbing owned by standalone FC tests or clearly transitional code that is removed or internalized before archive.
**Alternatives considered**: Canonize public pass-capture ABI/C++ wrappers as part of the extracted library contract; keep goggles visual tests dependent on private FC headers.
**Rationale**: Pass capture solves a testing problem, not a proven reusable-consumer problem. The extraction boundary should not permanently widen just to preserve goggles-side golden-test mechanics.

### Decision: Test ownership splits by responsibility boundary

**Choice**: Standalone `goggles-filter-chain` owns intermediate-pass golden tests and any library-level diagnostics/capture validation. Goggles keeps only end-to-end host integration coverage that proves the host can consume and drive FC correctly through the public boundary.
**Alternatives considered**: Keep all visual golden coverage in goggles; duplicate golden coverage in both repositories.
**Rationale**: Golden validation of FC internals and pass-by-pass execution belongs with the library that owns those mechanics. Goggles should validate host integration, not become the long-term owner of library-internal verification.

### Decision: Migration is incomplete until transitional boundary debt is removed

**Choice**: The change is not complete when the standalone repo merely exists or when goggles builds against it. Completion also requires removal of stale transitional APIs, shims, private-header escapes, outdated docs/spec language, and test arrangements that preserve the pre-extraction boundary.
**Alternatives considered**: Declare success after repo extraction and submodule switchover, leaving broader diagnostics/capture cleanup for later.
**Rationale**: Without cleanup, the extracted library would ship a caller-facing boundary shaped by temporary migration pressure instead of the intended durable contract.

### Decision: Goggles keeps the same `filter-chain/` integration path with local override support

**Choice**: Replace the tracked directory with a submodule at `filter-chain/` and keep `GOGGLES_FILTER_CHAIN_SOURCE_DIR` as the documented local override.
**Alternatives considered**: Move the dependency to `external/filter-chain/`; replace `add_subdirectory()` with a package-only workflow.
**Rationale**: This preserves the original integration plan, keeps build churn low, and does not conflict with the narrowed diagnostics boundary.

## Data Flow

### Phase 1-2 boundary convergence

```text
monorepo FC runtime + host boundary fixes
    |
    +--> move FC-owned namespaces and host type references to goggles::fc
    +--> remove host dependence on FC private/result headers
    +--> migrate goggles visual helpers off FC private headers
    |
    v
standalone extraction with reusable runtime boundary preserved
    |
    +--> FC standalone tests own pass-level/golden validation
    +--> public diagnostics surface narrowed to passive metadata/minimal policy
    +--> transitional pass-capture/session APIs cleaned up or internalized
```

### Standalone library ownership model

```text
host Vulkan handles + preset source + control requests
    |
    v
FC Instance -> Device -> Program -> Chain
    |
    +--> parse preset / resolve imports / compile shaders
    +--> build executable pass graph / manage history and resources
    +--> enumerate controls / apply caller mutations
    +--> record rendering work
    +--> expose bounded report/summary state
```

### Final repository/test ownership

```text
standalone goggles-filter-chain repo
    |
    +--> contract tests
    +--> consumer validation
    +--> intermediate-pass golden tests
    v
stable FC package/submodule consumed by goggles
    |
    +--> end-to-end host integration tests
    +--> clean-checkout submodule CI
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `filter-chain/include/goggles/filter_chain/vulkan_context.hpp` | Modify | Keep FC-owned C++ support types under `goggles::fc` and trim stale host-assumption fields during convergence if needed. |
| `filter-chain/include/goggles/filter_chain/filter_controls.hpp` | Modify | Preserve stable caller-facing control descriptors under `goggles::fc`. |
| `filter-chain/include/goggles/filter_chain.hpp` | Modify | Converge the canonical public C++20 entrypoint to the final stable runtime/inspection boundary; avoid treating pass capture as durable public API unless explicitly justified. |
| `filter-chain/include/goggles/filter_chain.h` | Modify | Narrow the canonical public C entrypoint to the final justified surface; remove transitional APIs that only exist to unblock migration tests. |
| `filter-chain/include/goggles_filter_chain.h` | Remove | Delete the legacy top-level C entrypoint with no compatibility shim. |
| `filter-chain/src/api/c_api.cpp` | Modify | Match the C ABI implementation to the narrowed diagnostics boundary and any internalized transitional helpers. |
| `filter-chain/src/api/cpp_wrapper.cpp` | Modify | Match RAII wrappers to the final public contract after cleanup. |
| `filter-chain/src/runtime/chain.hpp` | Modify | Keep diagnostics/capture machinery available internally as needed for library-owned tests without forcing it into the stable public boundary. |
| `filter-chain/src/runtime/chain.cpp` | Modify | Support passive summary metadata and any internal test-support capture flow needed by standalone golden tests. |
| `filter-chain/tests/contract/test_filter_chain.cpp` | Modify | Validate only the stable public contract that survives extraction cleanup. |
| `filter-chain/tests/contract/test_runtime_diagnostics.cpp` | Modify | Focus on the justified diagnostics surface; move any pass-capture expectations to internal or standalone golden-test coverage. |
| `filter-chain/tests/visual/**/*` or equivalent standalone golden-test area | Create/Modify | Own intermediate-pass golden tests in the standalone repo rather than goggles. |
| `tests/visual/runtime_capture.hpp` | Modify | Keep goggles helper limited to end-to-end host integration needs; remove dependence on FC-private headers and eventually any dependency on non-stable FC capture/session APIs. |
| `tests/visual/runtime_capture.cpp` | Modify | Use only the stable FC runtime boundary that goggles needs for host integration. If pass-level capture is still required during migration, treat it as temporary and remove before completion. |
| `tests/visual/test_intermediate_golden.cpp` | Delete or migrate | Move intermediate-pass golden ownership to the standalone FC repository. |
| `tests/visual/test_temporal_golden.cpp` | Modify or migrate | Keep in goggles only if it remains true host end-to-end coverage; otherwise move library-owned golden behavior to standalone FC tests. |
| `tests/visual/CMakeLists.txt` | Modify | Keep goggles visual targets limited to host integration coverage after migration. |
| `src/render/backend/vulkan_error.hpp` | Modify | Preserve host-owned error/result independence. |
| `src/render/backend/vulkan_debug.hpp` | Modify | Preserve host-owned error/result independence. |
| `src/render/backend/render_output.hpp` | Modify | Preserve host-owned error/result independence. |
| `src/render/backend/external_frame_importer.hpp` | Modify | Preserve host-owned error/result independence. |
| `CMakeLists.txt` | Modify | Keep the `filter-chain/` submodule bridge with local override support. |
| `.gitmodules` | Create/Modify | Point goggles to `git@github.com:goggles-dev/goggles-filter-chain.git` at the stable path. |
| `.github/workflows/ci.yml` | Modify | Initialize the FC submodule in goggles CI before configure/build/test. |
| `README.md`, `CONTRIBUTING.md`, issue templates, and standalone CI files in the extracted repo | Modify/Create | Document the final stable boundary and final testing ownership after transitional cleanup. |

## Interfaces / Contracts

### Stable FC public boundary

The stable public boundary for this change is centered on reusable runtime behavior:

- instance/device/program/chain lifecycle
- file and memory preset loading plus import/base-path handling
- record-time execution
- chain reports and last-error queries
- control enumeration, lookup, mutation, and justified reset helpers
- caller-facing runtime policy only where explicitly supported across hosts
  - retained stable runtime-policy helpers for this change are `goggles_fc_chain_set_stage_mask`, `goggles_fc_chain_set_prechain_resolution`, and `goggles_fc_chain_get_prechain_resolution`
  - retained stable reset helpers for this change are `goggles_fc_chain_reset_control_value` and `goggles_fc_chain_reset_all_controls`
- log routing hooks
- bounded passive diagnostics metadata only where justified

### Diagnostics contract

The design converges diagnostics to the narrowest justified stable shape:

- `mode` is the only diagnostics-policy control assumed stable by default.
- Any public diagnostics summary is passive metadata queried from chain state.
- Explicit diagnostic-session lifecycle is not presumed stable merely because current code exports it.
- If lifecycle APIs remain temporarily during migration, they are transitional and must be either justified and retained intentionally or removed/internalized before archive.

### Runtime policy and reset helper contract

The runtime-policy ambiguity for the local boundary migration is resolved as follows:

- `goggles_fc_chain_set_stage_mask` remains part of the stable public runtime surface because selective prechain/effect/postchain enablement is a reusable cross-host execution policy, not a Goggles-only diagnostics seam.
- `goggles_fc_chain_set_prechain_resolution` and `goggles_fc_chain_get_prechain_resolution` remain stable because prechain sizing is durable chain state that callers need to set and observe across retarget/rebuild flows.
- `goggles_fc_chain_reset_control_value` and `goggles_fc_chain_reset_all_controls` remain stable as the justified reset helpers referenced by the boundary design; they restore caller-visible control state without widening diagnostics or capture policy.
- No additional session-lifecycle, capture-oriented, or diagnostics-policy helpers are implied by retaining these APIs.

### Pass capture contract

Intermediate pass capture is treated as internal or test-support behavior unless a concrete non-test consumer requirement is documented during implementation. Therefore:

- goggles host code and goggles tests must not rely on FC private headers
- standalone FC tests may use internal/test-support seams to validate pass-by-pass results
- the extracted library's installed public contract must not promise pass capture as a durable consumer feature without explicit justification

### Host contract after extraction

Goggles remains responsible for:

- Vulkan instance/device selection and lifetime
- queue ownership, command submission, swapchain, and presentation
- external frame import and synchronization
- application-level policy and UI translation into FC inputs
- end-to-end host integration verification

FC remains responsible for:

- preset parsing and import resolution
- shader compilation/reflection
- executable pass graph and internal resources
- frame history and runtime control state
- deterministic runtime behavior behind the stable public API

## Testing Strategy

| Layer | What to Test | Approach |
|-------|--------------|----------|
| FC contract | Stable public runtime and bounded inspection surface | Standalone contract tests validate lifecycle, reports, controls, source loading, errors, and any intentionally retained diagnostics metadata. |
| FC golden/internal validation | Intermediate-pass behavior and pass-level artifacts | Standalone `goggles-filter-chain` tests own golden coverage for intermediate outputs and any internal capture-based assertions. |
| FC packaging | Consumer usability | Standalone CI continues to require static and shared consumer validation through installed packages. |
| Goggles integration | Host can consume FC through the stable boundary | Goggles tests build and run using only installed/submodule public FC headers and targets. |
| Goggles E2E visual | End-to-end host behavior only | Keep only coverage that proves goggles integration, presentation, and app-level flow; remove library-internal golden ownership from goggles. |
| Repo integration | Submodule and clean checkout behavior | Goggles CI initializes the submodule and passes full CI; standalone FC CI passes independently. |

## Migration / Rollout

### Phase 1: Prepare the monorepo

1. Complete the namespace migration and host reference updates to `goggles::fc`.
2. Preserve host-owned `Result<T>`/error independence.
3. Remove goggles dependence on FC private headers.
4. If temporary public diagnostics/session/capture affordances are needed to unblock migration work, isolate them as transitional rather than final boundary commitments.
5. Keep Phase 1 complete only when the monorepo is green under full CI.

Phase 1 gate remains `pixi run ci --runner container --cache-mode warm --lane all`.

### Phase 2: Create the standalone repository

1. Extract `filter-chain/` history into `goggles-dev/goggles-filter-chain`.
2. Add standalone build metadata, presets, CI, docs, and governance scaffolding.
3. Move intermediate-pass golden coverage and any test-support capture plumbing ownership into the standalone repository.
4. Converge standalone tests so they validate the runtime boundary directly, without depending on goggles-host test structure.
5. Update standalone docs to describe the narrowed stable diagnostics boundary.

Phase 2 is not complete until the standalone repo both builds independently and owns its library-level golden coverage.

### Phase 3: Consumer switchover in goggles

1. Replace the tracked `filter-chain/` directory with the standalone submodule at the same path.
2. Keep the local checkout override via `GOGGLES_FILTER_CHAIN_SOURCE_DIR`.
3. Remove or migrate goggles tests that still depend on transitional pass-capture/session-oriented FC APIs rather than true host integration behavior.
4. Update goggles CI and docs for submodule-aware clean checkouts.

Phase 3 is not complete until goggles uses FC through the same stable public boundary external consumers receive.

### Phase 4: Validation and cleanup

1. Re-run standalone CI and goggles full CI with the final submodule arrangement.
2. Tag `goggles-filter-chain` `v0.1.0` and pin goggles to that release.
3. Remove stale transitional APIs, wrappers, shims, tests, and docs that preserved a migration-only diagnostics/capture boundary.
4. Confirm specs, design, tasks, and repo documentation all describe the same final boundary and test ownership.
5. Archive the change only after both migration and cleanup are done.

No data migration is required, but contract migration and cleanup are mandatory.

## End-State Cleanup Requirements

The change remains open until all of the following are true:

- goggles no longer owns intermediate-pass golden tests that belong to FC runtime verification
- standalone `goggles-filter-chain` owns those golden tests and any internal capture support they require
- goggles visual/integration tests use only the final stable FC boundary needed for host integration
- no FC private headers are needed anywhere in goggles
- no stale public diagnostics/session/capture APIs remain merely because they were convenient during migration
- standalone and goggles docs describe the same final ownership boundary
- tasks/spec language that still implies a broader stable diagnostics surface is updated before archive

## Open Questions

- [ ] If any diagnostics lifecycle API remains public after cleanup, what non-goggles consumer use case justifies it as stable rather than transitional?
- [ ] Which existing goggles temporal visual checks remain true host end-to-end coverage and which should move fully into standalone FC golden coverage during Phase 2/3 cleanup?
