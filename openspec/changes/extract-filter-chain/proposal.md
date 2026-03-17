# Proposal: Extract filter-chain into standalone GitHub repository

## Problem

The `filter-chain/` sub-project has reached the point where its reusable Vulkan runtime, package/install surface, and independent validation story no longer fit cleanly inside the goggles monorepo. Keeping it embedded makes external consumption harder, couples its release cadence to the viewer, and leaves ownership boundaries blurry.

The extraction goal remains unchanged: move filter-chain into its own repository without weakening the host/library boundary. The accepted boundary is now tighter than some later transition planning assumed. FC should remain centered on reusable runtime mechanism plus bounded inspection. Intermediate pass capture is not a proven stable public API requirement, and any public diagnostics summary that survives should be passive metadata rather than a session-lifecycle-driven contract.

## Intent

Extract `filter-chain/` into `goggles-dev/goggles-filter-chain` as a first-class standalone library while preserving the original four-phase program:

1. **Reusability** - external Vulkan applications can consume FC without the goggles monorepo.
2. **Independent release cycle** - FC can ship and version independently, starting at `0.1.0`.
3. **Clear ownership boundary** - goggles keeps host integration responsibilities; FC owns runtime execution and its own diagnostics-heavy/intermediate-pass verification.

## Scope

### In Scope

- Namespace migration of FC-owned C++ types from `goggles::render` to `goggles::fc` across FC code and host callers.
- Removal of goggles host coupling to FC-owned `Result<T>` headers by switching host backend headers to `src/util/error.hpp`.
- Boundary convergence for diagnostics and test ownership: goggles host tests use only the durable public runtime boundary, while standalone `filter-chain` owns intermediate-pass golden coverage and other diagnostics-heavy verification.
- Stable caller-facing diagnostics policy narrowed to the minimum justified surface; any retained summary/readout remains passive metadata, not a public session-management promise.
- `FilterChainDependencies.cmake` portability fixes so standalone FC resolves dependencies through standard CMake discovery.
- Asset audit to ensure only `crt-lottes-fast` remains in upstream packaged presets while required test/diagnostic/internal shader assets stay intact.
- Standalone repository creation at `git@github.com:goggles-dev/goggles-filter-chain.git` via `git filter-repo --subdirectory-filter filter-chain/`, including standalone `pixi.toml`, `CMakePresets.json`, CI, license, docs, and repository governance.
- Goggles consumer switchover from tracked in-repo `filter-chain/` content to a pinned git submodule while preserving `add_subdirectory()` semantics.
- Final migration and stale transitional code cleanup across both repos before the change is considered complete.

### Out of Scope

- Rolling back, replacing, or de-scoping the standalone extraction itself.
- Treating public intermediate-pass capture as part of the durable stable FC API without a later separately justified change.
- Expanding public diagnostics/session lifecycle APIs beyond the minimum stable inspection surface required by the accepted boundary.
- Shared error/util library extraction.
- Migration from git submodule consumption to conda/pixi packaging.
- Unrelated FC feature work or broad host/render refactors beyond what extraction requires.

## Approach

The extraction remains a four-phase process. The phase structure stays intact, but the accepted boundary is tightened inside that plan.

### Phase 1: Prepare the monorepo

Land all boundary-hardening work in goggles first so the existing CI validates the extracted shape before the split.

1. **Namespace migration** - rename FC-owned C++ namespaces from `goggles::render` to `goggles::fc` across FC code, tests, and host references.
2. **Host error-type decoupling** - stop goggles backend headers from depending on FC-owned result/error headers.
3. **Host/test boundary cleanup** - remove goggles reliance on FC private headers and migrate goggles-side verification to the durable public runtime boundary only.
4. **Diagnostics boundary tightening** - stop treating intermediate pass capture as a stable caller-facing requirement; if diagnostics summary remains public, treat it as passive inspection metadata rather than public session lifecycle control.
5. **Verification ownership split** - move intermediate-pass golden coverage and other diagnostics-heavy verification into standalone `filter-chain` ownership; goggles keeps end-to-end host integration coverage.
6. **Portability and asset audit** - make dependency discovery standalone-safe and confirm packaged shader contents match the curated asset boundary.

**Gate**: Full monorepo CI green (`pixi run ci --runner container --cache-mode warm --lane all`).

### Phase 2: Create standalone repository

Extract clean history and make FC independently buildable, testable, documented, and governable.

1. Run `git filter-repo --subdirectory-filter filter-chain/` from a fresh clone.
2. Push to `git@github.com:goggles-dev/goggles-filter-chain.git`.
3. Add standalone repo infrastructure: `pixi.toml`, `CMakePresets.json`, CI workflow, `LICENSE`, `README.md`, issue templates, and repository settings.
4. Keep FC's own verification authoritative for runtime internals, including diagnostics-heavy and intermediate-pass golden workflows that no longer belong to goggles-host coverage.
5. Version the standalone project independently as `0.1.0`.

**Gate**: Standalone CI green, including build/test, consumer validation, static analysis, and standalone-owned verification coverage.

### Phase 3: Consumer switchover in goggles

Replace the monorepo-owned FC tree with consumption of the extracted repository.

1. Remove the tracked `filter-chain/` directory from goggles.
2. Add `goggles-filter-chain` back as a git submodule at `filter-chain/`.
3. Keep `add_subdirectory()` integration, with an overrideable local checkout path for co-development.
4. Update goggles CI and path-sensitive docs/scripts so clean checkouts initialize the submodule before build/test.
5. Verify goggles host integration coverage still passes against the extracted dependency.

**Gate**: Goggles CI green with submodule integration (`pixi run ci --runner container --cache-mode warm --lane all`).

### Phase 4: Validation and cleanup

Complete the extraction by finishing migration, removing stale transitional scaffolding, and validating both repositories in their final state.

1. Tag `goggles-filter-chain` `v0.1.0` and pin goggles to that release.
2. Remove or internalize stale transitional code, wrappers, API surface, and docs that existed only to bridge pre-extraction goggles-specific test needs.
3. Confirm no in-tree caller still depends on transitional diagnostics/session/capture paths that are outside the accepted stable boundary.
4. Re-run final standalone and goggles verification from clean checkouts.
5. Archive the change only after migration and cleanup are both complete.

**Gate**: Both repos green, release tag published, goggles pinned to the tagged submodule ref, and stale transitional extraction scaffolding removed.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `filter-chain/include/goggles/filter_chain/*.hpp` | Modified | FC namespace boundary and durable public C++ surface updates |
| `filter-chain/include/goggles/filter_chain.h` | Modified | Canonical public C entrypoint narrowed to the minimum justified runtime/inspection surface |
| `filter-chain/include/goggles/filter_chain.hpp` | Modified | Canonical public C++20 entrypoint exposes the installed wrapper surface |
| `filter-chain/include/goggles_filter_chain.h` | Removed | Legacy top-level C entrypoint removed with no compatibility shim |
| `filter-chain/src/**/*.{hpp,cpp}` | Modified | Runtime extraction, namespace migration, diagnostics/capture internalization, and cleanup |
| `filter-chain/tests/**/*` | Modified | Standalone ownership of diagnostics-heavy and intermediate-pass verification |
| `filter-chain/cmake/FilterChainDependencies.cmake` | Modified | Standard CMake dependency discovery for standalone use |
| `filter-chain/assets/shaders/**/*` | Modified/Reviewed | Curated packaged shader contents and required internal/test assets |
| `src/render/backend/**/*` | Modified | Host boundary cleanup, FC type namespace updates, and submodule consumption support |
| `tests/render/**/*` | Modified | Host integration tests aligned to durable public FC surface only |
| `tests/visual/**/*` | Modified | Goggles visual coverage narrowed to end-to-end host behavior rather than intermediate artifact ownership |
| `CMakeLists.txt` | Modified | Submodule-based `add_subdirectory()` bridge with local override support |
| `.gitmodules` | New | Git submodule configuration for extracted FC repository |
| `.github/workflows/ci.yml` | Modified | Goggles CI submodule initialization and extracted-boundary validation |
| `openspec/changes/extract-filter-chain/specs/**/*.md` | Modified | Change specs updated to match tightened diagnostics boundary and verification ownership split |
| `openspec/specs/**/*.md` | Modified | Living specs updated where extraction changes the durable system contract |

## Non-goals

- Preserve transitional testing-oriented API surface just because it unblocked goggles during migration.
- Make goggles the long-term owner of FC intermediate-pass artifact generation or golden baselines.
- Expand the public diagnostics contract beyond what is justified for external consumers.

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Boundary convergence stalls and leaves transition-era diagnostics/capture APIs treated as permanent | Medium | Make cleanup an explicit completion gate and align proposal/spec/design/tasks around the narrowed durable boundary |
| Goggles host coverage loses useful debugging visibility when intermediate workflows move out | Medium | Keep goggles end-to-end integration assertions, but relocate intermediate-pass and diagnostics-heavy golden coverage into standalone FC verification where it belongs |
| Namespace and dependency-boundary changes break builds in unexpected places | Medium | Land all prep work in the monorepo first and gate Phase 1 with full CI |
| Two-repo development and submodule adoption introduce workflow friction | Medium | Keep stable `filter-chain/` path, support local override, and document the co-development workflow in both repos |
| Extraction appears done before migration debt is actually removed | High | Treat stale transitional code/docs/API cleanup as mandatory Phase 4 work, not optional follow-up |

## Rollback Plan

Each phase remains independently reversible:

- **Phase 1** - revert the monorepo preparation commits that rename namespaces and harden the boundary.
- **Phase 2** - delete the new standalone repository if extraction infrastructure is not ready.
- **Phase 3** - remove the submodule, restore the in-repo `filter-chain/` tree from the last pre-switchover commit, and revert the goggles build/CI changes.
- **Phase 4** - revert cleanup commits and retag/repin as needed if final convergence exposes regressions.

Rollback restores a working monorepo state, but Phase 4 cleanup is not optional in the forward direction.

## Dependencies

- GitHub repository creation and admin access for `goggles-dev/goggles-filter-chain`.
- `git-filter-repo` availability.
- Agreement that standalone `filter-chain` owns intermediate-pass golden tests and diagnostics-heavy runtime verification.
- Follow-through to update both change artifacts and living specs so the accepted boundary is consistent across proposal, design, specs, tasks, and code.

## Validation Plan

- Run full goggles CI after monorepo preparation (`pixi run ci --runner container --cache-mode warm --lane all`).
- Run standalone FC CI after extraction, including build/test, static analysis, and installed-consumer validation.
- Verify goggles clean-checkout + submodule initialization succeeds and full goggles CI passes against the extracted dependency.
- Verify no remaining caller-facing dependency on transitional diagnostics session/capture surface that falls outside the accepted stable boundary.
- Verify migration is not declared complete until stale transitional code and docs are removed.

## Success Criteria

- [ ] FC is extracted to `goggles-dev/goggles-filter-chain` with independent versioning, CI, docs, and governance.
- [ ] FC-owned C++ types use `goggles::fc`, and goggles host code no longer depends on FC-owned result/error headers.
- [ ] Goggles host/tests consume only the durable public FC boundary; no goggles-owned test relies on FC private headers or owns intermediate-pass golden workflows.
- [ ] Intermediate-pass golden tests and diagnostics-heavy verification live under standalone `filter-chain` ownership.
- [ ] The stable caller-facing diagnostics policy is narrowed to the minimum justified surface, and any retained public summary is passive metadata rather than session-lifecycle-driven API control.
- [ ] Goggles consumes FC via a pinned submodule and passes full CI from a clean checkout.
- [ ] Standalone FC passes its own required CI and consumer validation gates.
- [ ] Migration is complete only after stale transitional code, wrappers, docs, and boundary scaffolding are cleaned up across both repos.
