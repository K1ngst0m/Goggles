# Exploration: Extract filter-chain into standalone GitHub repository

## Current State

The `filter-chain/` directory in the goggles monorepo is already structured as a near-standalone CMake sub-project:

- **Own `project(GogglesFilterChain VERSION 0.1.0)`** with full install targets, package config generation, and version compatibility
- **Clean C ABI** (40+ functions via `src/api/c_api.cpp`) + C++ RAII wrappers (`src/api/cpp_wrapper.cpp`)
- **Consumer validation tests**: C11, static C++, and shared C++ consumers in `tests/consumer/`
- **Embedded asset pipeline**: Internal shaders are compiled into the binary via `EmbedAssets.cmake` - no runtime asset directory needed
- **Independent infrastructure**: Own `.clang-format`, `.clang-tidy`, CMake modules (`cmake/CompilerConfig.cmake`, `cmake/CodeQuality.cmake`, `cmake/FilterChainDependencies.cmake`)
- **Public headers**: Canonical C++ entrypoint `include/goggles/filter_chain.hpp` plus 6 support headers in `include/goggles/filter_chain/` (`common.hpp`, `error.hpp`, `filter_controls.hpp`, `result.hpp`, `scale_mode.hpp`, `vulkan_context.hpp`)
- **Duplicated shared types**: `goggles::Error`, `goggles::Result<T>`, `goggles::ErrorCode` defined identically in both `filter-chain/include/goggles/filter_chain/error.hpp` and `src/util/error.hpp` with `GOGGLES_ERROR_TYPES_DEFINED` ODR guards

### How goggles monorepo currently consumes filter-chain

The top-level `CMakeLists.txt` simply does `add_subdirectory(filter-chain)`. The host project accesses FC through:

1. **C++ wrapper API**: `src/render/backend/filter_chain_controller.hpp` includes `<goggles_filter_chain.hpp>`
2. **Public headers**: `vulkan_context.hpp`, `filter_controls.hpp` for cross-boundary types
3. **Result<T> types**: 4 host files (`vulkan_error.hpp`, `vulkan_debug.hpp`, `external_frame_importer.hpp`, `render_output.hpp`) include `<goggles/filter_chain/result.hpp>` instead of `src/util/error.hpp`

### Dependencies

FC depends on (via `FilterChainDependencies.cmake`):
- **Public**: Vulkan SDK, expected-lite (nonstd::expected)
- **Private**: spdlog, slang (shader compiler), stb_image, Catch2 (tests only)
- All currently provided through pixi/conda (`$CONDA_PREFIX` hints in cmake)
- `expected-lite` is a local pixi package in `packages/expected-lite/` (recipe builds from upstream git)

## Affected Areas

### In the new standalone repo (to be created)
- `filter-chain/` entire directory moves to become the repo root
- Needs: own `pixi.toml`, `CMakePresets.json`, CI workflow, LICENSE, README
- Namespace: internal code uses `goggles::render` extensively (47 matches in private headers)
- Public headers use `goggles::render` for `VulkanContext` and `FilterControlDescriptor`

### In the goggles monorepo (post-extraction)
- `CMakeLists.txt`: Replace `add_subdirectory(filter-chain)` with external dependency consumption
- `cmake/Dependencies.cmake`: Add `find_package(GogglesFilterChain)` or equivalent
- `src/render/backend/`: 4 files using `<goggles/filter_chain/result.hpp>` should switch to `src/util/error.hpp`
- `tests/visual/runtime_capture.cpp`: Includes FC internal headers (`chain/chain_runtime.hpp`, `diagnostics/diagnostic_policy.hpp`) directly
- `tests/render/test_filter_boundary_contracts.cpp`, `test_vulkan_backend_subsystem_contracts.cpp`: Include FC headers
- CI scripts: Consumer validation test would move to the new repo

## Approaches

### 1. Git History Extraction

#### A. `git filter-repo` (Recommended)
- Pros: Clean rewrite, preserves commit history for `filter-chain/` subtree, fastest tool
- Cons: Rewrites SHAs (expected), requires `pip install git-filter-repo`
- Effort: Low
- Method: `git filter-repo --subdirectory-filter filter-chain/` on a clone, then push to new remote

#### B. `git subtree split`
- Pros: Built into git, no external tools, creates clean linear history
- Cons: Loses merge commits, can be slow on large repos
- Effort: Low
- Method: `git subtree split --prefix=filter-chain/ -b filter-chain-standalone && git push <new-remote> filter-chain-standalone:main`

#### C. Fresh repo (copy, no history)
- Pros: Cleanest start, no historical baggage
- Cons: Loses valuable commit history and blame context
- Effort: Low
- Method: Copy directory, `git init`, commit

**Recommendation**: Option A (`git filter-repo`) - standard tool for this, preserves meaningful history.

### 2. Consumer Integration (How goggles consumes the extracted library)

#### A. Git submodule
- Pros: Exact version pinning, works well with `add_subdirectory()`, minimal CMake changes
- Cons: Submodule workflow friction, nested git repos, clone complexity
- Effort: Low (minimal CMake changes)

#### B. CMake FetchContent
- Pros: Automatic download at configure time, no submodule friction, version pinning via git tag
- Cons: Download at configure time, no offline builds without cache, harder to develop both simultaneously
- Effort: Medium

#### C. Pixi/conda package
- Pros: Consistent with existing dependency model, clean package boundary, versioned releases
- Cons: Requires packaging infrastructure (recipe.yaml), publish workflow, version lag during development
- Effort: High (need conda package build + publish pipeline)

#### D. Vendored copy (manual or scripted)
- Pros: Simple, offline-capable, full control
- Cons: Manual sync burden, no version tracking
- Effort: Low initially, high ongoing

**Recommendation**: Option A (git submodule) for initial extraction - least friction, allows `add_subdirectory()` to keep working exactly as today. Migrate to Option C (pixi/conda package) once the library stabilizes and a release cadence is established.

### 3. Standalone CI Pipeline

The new repo needs its own CI workflow. Based on the goggles CI pattern:

```
Jobs:
1. format-check        - clang-format + taplo (if TOML files present)
2. build-and-test      - cmake build + ctest (debug, asan presets)
3. consumer-validation - validate-installed-consumers.sh (C, static C++, shared C++)
4. static-analysis     - clang-tidy quality build
```

Key decisions:
- Needs own `pixi.toml` with FC-specific dependencies only (Vulkan, spdlog, slang, expected-lite, stb, Catch2, clang-tools)
- Needs own `CMakePresets.json` (can start with subset of goggles presets)
- Consumer validation is already self-contained in `scripts/task/validate-installed-consumers.sh` and `tests/consumer/`
- No Semgrep rules needed initially (those are goggles-app-specific)

### 4. Packaging Format

#### A. Conda package via pixi-build
- Pros: Matches goggles dependency model, existing `packages/` pattern as template
- Cons: Need rattler-build recipe, publish to conda-forge or private channel
- Effort: Medium

#### B. CMake install + find_package (FetchContent or submodule)
- Pros: Already works (`GogglesFilterChainConfig.cmake.in` fully functional), standard C++ approach
- Cons: Not a "package" in the distribution sense
- Effort: Already done

#### C. System packages (deb/rpm)
- Pros: Standard for Linux distribution
- Cons: Heavy infrastructure, narrow user base for now
- Effort: High

**Recommendation**: Start with B (CMake install - already works). Add A (conda package) when the library needs wider distribution. The existing `GogglesFilterChainConfig.cmake.in` is production-ready.

## Coupling Issues (Pre-extraction vs Post-extraction)

### Fix BEFORE extraction
1. **Result<T> coupling** (4 host files): These should include `src/util/error.hpp` instead of `<goggles/filter_chain/result.hpp>`. Both files are identical with ODR guards, but the host should use its own copy. Quick fix, reduces coupling score.

2. **Visual test internal header includes**: `tests/visual/runtime_capture.cpp` includes `chain/chain_runtime.hpp` and `diagnostics/diagnostic_policy.hpp` (FC private headers). This will break after extraction. Needs refactoring to use only the public API.

### Fix AFTER extraction (or as part of it)
3. **`$CONDA_PREFIX` hints**: These are fine for now - they'll work in the standalone pixi environment too. Can be cleaned up later with proper CMake find module paths.

4. **Namespace `goggles::render`**: This is internal to FC and doesn't affect the extraction. The public API uses C linkage. Renaming to `goggles::filter_chain` would be a large internal refactor with no functional benefit for extraction.

5. **Missing standalone infrastructure**: pixi.toml, CMakePresets.json, CI workflow, LICENSE - these are created as part of the extraction, not prerequisites.

## Key Risks

1. **Visual test breakage**: `tests/visual/runtime_capture.cpp` directly includes FC internal headers. After extraction, these internal headers won't be available. The visual tests need refactoring to use only public FC API before or during extraction.

2. **ODR fragility**: The shared `goggles::Error`/`goggles::Result<T>` types are guarded by `GOGGLES_ERROR_TYPES_DEFINED`, but if the types ever diverge between the two copies, hard-to-debug ODR violations will occur. The extraction is an opportunity to decide: does FC export its own error types, or does it share a common error library?

3. **CI parity gap**: Until the standalone repo has its own CI, there's a window where FC changes aren't validated independently. The goggles monorepo CI currently validates FC as part of its build.

4. **`expected-lite` packaging**: FC publicly depends on `nonstd::expected`. Currently provided by the goggles local pixi package `packages/expected-lite/`. The standalone repo needs its own way to get this dependency (own pixi recipe, or rely on conda-forge).

5. **Slang compiler dependency**: The `slang` shader compiler is a private dependency provided by pixi. Need to verify it's available on conda-forge or package it for the standalone repo.

6. **Two-repo development friction**: During active development, changes that span FC and goggles will require coordinated commits across two repos. Git submodules help but add workflow complexity.

## Questions Requiring User Input

See structured questions below - these cover the key decisions that need user input before a concrete proposal can be created.

## Ready for Proposal

**No** - Several key decisions need user input first:
1. Git history preservation strategy
2. Consumer integration method
3. Namespace/error type strategy
4. Versioning and release cadence
5. Whether to fix coupling issues before or after extraction
6. License for the new repo
7. CI/issue tracker decisions
