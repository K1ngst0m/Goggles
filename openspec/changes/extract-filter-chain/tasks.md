# Tasks: Extract filter-chain into standalone GitHub repository

## Phase 1: Prepare Monorepo

- [x] T1.1 Rename FC-owned namespaces inside filter-chain while preserving the public C ABI
  - Description: Mechanically migrate FC-owned C++ namespaces from `goggles::render` to `goggles::fc` across FC public headers, internal headers, implementation files, runtime wrappers, and contract tests. Keep the exported C ABI symbol set unchanged after the rename.
  - Files involved: `filter-chain/include/goggles/filter_chain.h`, `filter-chain/include/goggles/filter_chain/vulkan_context.hpp`, `filter-chain/include/goggles/filter_chain/filter_controls.hpp`, `filter-chain/src/**/*.hpp`, `filter-chain/src/**/*.cpp`, `filter-chain/tests/contract/*.cpp`
  - Verification method: `rg -n "goggles::render" filter-chain/include filter-chain/src filter-chain/tests/contract`; build a shared FC library before and after the rename and compare `nm -D --defined-only <before-lib> | sort` vs `nm -D --defined-only <after-lib> | sort`; `cmake --preset asan`; `cmake --build --preset asan`; `ctest --preset asan --output-on-failure`
  - Dependencies: None
  - Estimated complexity: L

- [x] T1.2 Update every goggles host/test namespace reference to FC-owned public types and helpers
  - Description: Replace host-side references to FC-owned symbols with `goggles::fc::...`, including unqualified `FilterControl*` usages in `filter_chain_controller.hpp`, `render::FilterControl*` usages in the app/UI files, `goggles::render::VulkanContext`, `make_filter_control_id()`, `to_string()`, and `clamp_filter_control_value()` call sites. Leave host-owned `goggles::render` types unchanged.
  - Files involved: `src/render/backend/vulkan_context.hpp`, `src/render/backend/vulkan_context.cpp`, `src/render/backend/filter_chain_controller.hpp`, `src/render/backend/filter_chain_controller.cpp`, `src/app/application.cpp`, `src/ui/imgui_layer.hpp`, `src/ui/imgui_layer.cpp`, `tests/render/test_filter_boundary_contracts.cpp`, `tests/render/test_vulkan_backend_subsystem_contracts.cpp`, `tests/render/test_filter_chain_retarget.cpp`
  - Verification method: `rg -n "goggles::render::(VulkanContext|FilterControlDescriptor|FilterControlId|FilterControlStage|make_filter_control_id|to_string|clamp_filter_control_value)|render::FilterControl(Descriptor|Id|Stage)" src tests/render`; verify `clamp_filter_control_value()` call sites compile after the namespace migration and that the existing clamping-behavior unit coverage in `filter-chain/tests/contract/test_filter_controls.cpp` passes unchanged; `cmake --preset asan`; `cmake --build --preset asan --target goggles_render test_filter_controls test_filter_boundary_contracts test_vulkan_backend_subsystem_contracts test_filter_chain_retarget`; `ctest --preset asan --output-on-failure --tests-regex "filter_controls|filter_boundary|vulkan_backend_subsystem|filter_chain_retarget"`
  - Dependencies: T1.1
  - Estimated complexity: M

- [x] T1.3 Migrate semgrep fixtures that model FC-owned types to the new namespace
  - Description: Inspect `filter-chain/tests/semgrep/` fixtures and update any fixture source that still models FC-owned types under `goggles::render` so the fixtures track the extracted `goggles::fc` ownership boundary.
  - Files involved: `filter-chain/tests/semgrep/**/*`, `tests/semgrep/fixtures/src/render/chain/**/*`
  - Verification method: `rg -n "goggles::render::(VulkanContext|FilterControlDescriptor|FilterControlId|FilterControlStage|make_filter_control_id|clamp_filter_control_value)" filter-chain/tests/semgrep tests/semgrep/fixtures/src/render/chain`; `pixi run ci --runner container --cache-mode warm --lane all`
  - Dependencies: T1.1
  - Estimated complexity: S

- [x] T1.4 Remove goggles host dependence on FC-owned result/error headers
  - Description: Replace `<goggles/filter_chain/result.hpp>` with `util/error.hpp` in the four backend headers that only need host-owned `Result<T>` definitions, preserving the ODR guard behavior expected by the specs.
  - Files involved: `src/render/backend/vulkan_error.hpp`, `src/render/backend/vulkan_debug.hpp`, `src/render/backend/render_output.hpp`, `src/render/backend/external_frame_importer.hpp`
  - Verification method: `rg -n "goggles/filter_chain/result.hpp" src/render/backend`; `cmake --preset asan`; `cmake --build --preset asan --target goggles_render`
  - Dependencies: None
  - Estimated complexity: S

- [x] T1.5 Converge the FC public diagnostics boundary to the minimum justified stable surface
  - Description: Update the installed FC C and C++ API so the stable caller-facing diagnostics contract is limited to reusable runtime mechanism plus bounded inspection. Keep only the minimum justified policy surface, ensure any retained summary/readout is passive metadata queried from chain state, and stop treating diagnostics-session lifecycle control or intermediate pass capture as accepted stable end-state API. The installed package story SHALL converge on canonical language-binding entrypoints at `goggles/filter_chain.h` and `goggles/filter_chain.hpp`, with no compatibility shim for `goggles_filter_chain.h`.
  - Files involved: `filter-chain/include/goggles/filter_chain.h`, `filter-chain/include/goggles/filter_chain.hpp`, `filter-chain/src/api/c_api.cpp`, `filter-chain/src/api/cpp_wrapper.cpp`, `filter-chain/src/runtime/chain.hpp`, `filter-chain/src/runtime/chain.cpp`, `filter-chain/tests/contract/test_filter_chain.cpp`, `filter-chain/tests/contract/test_runtime_diagnostics.cpp`
  - Verification method: `rg -n "diagnostic_session|capture_pass_output|get_diagnostic_summary|diagnostic_mode" filter-chain/include filter-chain/src filter-chain/tests/contract`; verify public contract tests assert passive metadata/report access rather than public session lifecycle; `cmake --preset asan`; `cmake --build --preset asan --target test_runtime_diagnostics test_filter_chain`; `ctest --preset asan --output-on-failure --tests-regex "runtime_diagnostics|filter_chain"`
  - Dependencies: T1.1
  - Estimated complexity: L

- [x] T1.6 Rewrite goggles-side FC helpers and host tests around the durable public boundary only
  - Description: Keep `tests/visual/runtime_capture.*`, `tests/render/*`, and related visual CMake wiring limited to installed/public FC headers and the final durable runtime boundary used by host integration. Any temporary reliance on migration-only diagnostics/session/capture seams must be isolated, documented as transitional, and scheduled for removal before Phase 4 completes; goggles-owned coverage must no longer imply stable caller-facing pass capture.
  - Files involved: `tests/visual/runtime_capture.hpp`, `tests/visual/runtime_capture.cpp`, `tests/visual/CMakeLists.txt`, `tests/visual/test_temporal_golden.cpp`, `tests/render/test_filter_boundary_contracts.cpp`, `tests/render/test_vulkan_backend_subsystem_contracts.cpp`, `tests/render/test_filter_chain_retarget.cpp`
  - Verification method: `rg -n "chain_runtime.hpp|diagnostic_policy.hpp|diagnostic_session.hpp|test_harness_sink.hpp|filter-chain/src" tests/visual tests/render`; inspect goggles-side FC assertions for passive metadata/report usage only; `cmake --preset asan`; `cmake --build --preset asan --target test_temporal_golden test_filter_boundary_contracts test_vulkan_backend_subsystem_contracts test_filter_chain_retarget`; `ctest --preset asan --output-on-failure --tests-regex "temporal_golden|filter_boundary|vulkan_backend_subsystem|filter_chain_retarget"`
  - Dependencies: T1.2, T1.5
  - Estimated complexity: L

- [x] T1.7 Portabilize `FilterChainDependencies.cmake` for standalone use
  - Description: Remove Conda/Pixi-specific path hints from FC dependency discovery and switch the module to standard `find_package()` / `find_path()` resolution so the extracted project configures from normal CMake search paths.
  - Files involved: `filter-chain/cmake/FilterChainDependencies.cmake`
  - Verification method: `rg -n "CONDA_PREFIX|CONDA_BUILD_SYSROOT|ENV\{CONDA" filter-chain/cmake/FilterChainDependencies.cmake`; `cmake -S filter-chain -B build/filter-chain-standalone`; `cmake --build build/filter-chain-standalone`
  - Dependencies: None
  - Estimated complexity: M

- [x] T1.8 Audit FC asset contents and prove required shader classes remain packaged
  - Description: Confirm the standalone asset set retains exactly one upstream preset (`crt-lottes-fast`) while still carrying every required test shader, diagnostic shader, and internal blit/downsample shader into the embedded asset payload.
  - Files involved: `filter-chain/assets/shaders/upstream/**/*`, `filter-chain/assets/shaders/**/*`, `filter-chain/tests/contract/test_shader_validation.cpp`, `filter-chain/tests/contract/test_runtime_diagnostics.cpp`, `filter-chain/tests/contract/test_zfast_integration.cpp`
  - Verification method: enumerate upstream presets and confirm only `crt-lottes-fast` remains; verify test/diagnostic/internal shader directories still contain the files referenced by contract tests; `cmake --preset asan`; `cmake --build --preset asan --target test_shader_validation test_runtime_diagnostics test_zfast_integration`; `ctest --preset asan --output-on-failure --tests-regex "shader_validation|runtime_diagnostics|zfast_integration"`
  - Dependencies: None
  - Estimated complexity: M

- [x] T1.9 Run the monorepo preparation gate against the narrowed end-state boundary
  - Description: Revalidate the prepared monorepo only after the diagnostics-boundary convergence and goggles-host migration tasks are updated to the accepted end state. Phase 1 is complete only when the monorepo is green without requiring goggles-owned stable pass capture or public diagnostics-session lifecycle assumptions.
  - Files involved: CI output only
  - Verification method: `pixi run ci --runner container --cache-mode warm --lane all`
  - Dependencies: T1.3, T1.4, T1.5, T1.6, T1.7, T1.8
  - Estimated complexity: M

## Phase 2: Create Standalone Repository

- [x] T2.1 Extract filter-chain history into a standalone repository root
  - Description: Clone from a fresh monorepo checkout, run `git filter-repo --subdirectory-filter filter-chain/`, rename the old remote, add `git@github.com:goggles-dev/goggles-filter-chain.git`, and verify the new repository contains only FC-root-relative history.
  - Files involved: extracted repository root, git history, git remotes
  - Verification method: `git log --stat`; `git remote -v`; `test -f CMakeLists.txt`; `test -d src`; `test -d include`; `test ! -d filter-chain`
  - Dependencies: T1.9
  - Estimated complexity: M

- [x] T2.2 Establish standalone build metadata, versioning, and local Pixi package carry-over
  - Description: Add the standalone `pixi.toml`, `CMakePresets.json`, and `project(GogglesFilterChain VERSION 0.1.0)` updates, and carry over the local Pixi package recipes required by the design (`packages/expected-lite/`, `packages/stb/`, `packages/slang-shaders/`) so the extracted repo is self-contained.
  - Files involved: `pixi.toml`, `CMakePresets.json`, `CMakeLists.txt`, `packages/expected-lite/**/*`, `packages/stb/**/*`, `packages/slang-shaders/**/*`
  - Verification method: `test -d packages/expected-lite`; `test -d packages/stb`; `test -d packages/slang-shaders`; `pixi install`; `cmake --list-presets`; `cmake --preset debug`; `cmake --preset release`; `cmake --preset asan`; `cmake --preset quality`; `cmake --preset test`; `rg -n "include.*goggles|configurePresets.*goggles" CMakePresets.json` returns no matches so the standalone `CMakePresets.json` is self-contained and does not include goggles presets
  - Dependencies: T2.1
  - Estimated complexity: L

- [x] T2.3 Move standalone-owned golden and diagnostics-heavy verification into the extracted repository
  - Description: Migrate intermediate-pass golden coverage and any diagnostics-heavy runtime verification that depends on internal capture-oriented capabilities out of goggles and into standalone FC-owned targets. Keep any required capture plumbing internal or clearly non-stable within the standalone repo rather than widening the installed public contract.
  - Files involved: `tests/visual/test_intermediate_golden.cpp` (source of migrated coverage), `filter-chain/tests/visual/**/*`, `filter-chain/tests/contract/test_runtime_diagnostics.cpp`, `filter-chain/src/runtime/chain.hpp`, `filter-chain/src/runtime/chain.cpp`, standalone `CMakeLists.txt`
  - Verification method: verify intermediate-pass/golden targets exist only in standalone FC build files; `rg -n "intermediate_golden|capture_pass_output|diagnostic_session" filter-chain/tests filter-chain/src`; `cmake --preset test`; `cmake --build --preset test`; `ctest --preset test --output-on-failure --tests-regex "intermediate|golden|runtime_diagnostics"`
  - Dependencies: T2.1, T2.2
  - Estimated complexity: L

- [x] T2.4 Rewrite installed-consumer validation for the standalone repo layout
  - Description: Update `scripts/validate-installed-consumers.sh` to use repository-root-relative paths after extraction, preserve the `tests/consumer/{c_api,static,shared}` fixtures in the standalone tree, and require both static and shared install validation with no skip path.
  - Files involved: `scripts/validate-installed-consumers.sh`, `tests/consumer/c_api/**/*`, `tests/consumer/static/**/*`, `tests/consumer/shared/**/*`
  - Verification method: `rg -n "filter-chain/|\$REPO_ROOT/filter-chain|tests/consumer" scripts/validate-installed-consumers.sh`; `bash scripts/validate-installed-consumers.sh --preset test`
  - Dependencies: T2.1, T2.2
  - Estimated complexity: M

- [x] T2.5 Add standalone CI lanes and ignore rules for the extracted layout and new verification ownership
  - Description: Create or update the standalone GitHub Actions workflow with `format-check`, `build-and-test`, `consumer-validation`, and `static-analysis` jobs, and make the standalone pipeline the authoritative home for intermediate-pass golden and diagnostics-heavy verification that no longer belongs to goggles host CI.
  - Files involved: `.github/workflows/ci.yml`, `.semgrepignore`, `pixi.toml`, `CMakePresets.json`, standalone `CMakeLists.txt`, `filter-chain/tests/visual/**/*`
  - Verification method: `rg -n "format-check|build-and-test|consumer-validation|static-analysis" .github/workflows/ci.yml`; `rg -n "intermediate|golden|runtime_diagnostics" .github/workflows/ci.yml CMakePresets.json`; `pixi run format-check`; `cmake --preset quality`; `cmake --build --preset quality`; `bash scripts/validate-installed-consumers.sh --preset test`
  - Dependencies: T2.3, T2.4
  - Estimated complexity: M

- [x] T2.6 Add standalone repository documentation and issue scaffolding that describe the final boundary
  - Description: Create or update `README.md`, `LICENSE`, `CONTRIBUTING.md`, and GitHub issue templates so they document builds, tests, `find_package(GogglesFilterChain CONFIG REQUIRED)`, the local goggles override flow, the narrowed stable diagnostics boundary, and the fact that standalone FC owns intermediate-pass golden coverage.
  - Files involved: `README.md`, `LICENSE`, `CONTRIBUTING.md`, `.github/ISSUE_TEMPLATE/**/*`, `CMakeLists.txt`
  - Verification method: `rg -n "find_package\(GogglesFilterChain|GOGGLES_FILTER_CHAIN_SOURCE_DIR|0.1.0|MIT|intermediate-pass|diagnostics|consumer-validation|static-analysis" README.md CONTRIBUTING.md LICENSE CMakeLists.txt`; `test -d .github/ISSUE_TEMPLATE`
  - Dependencies: T2.3, T2.5
  - Estimated complexity: M

- [x] T2.7 Run the standalone validation gate with final verification ownership in place
  - Description: Prove the extracted repository is independently buildable, testable, and consumable, and that standalone FC now owns the intermediate-pass golden and diagnostics-heavy verification previously carried by goggles.
  - Files involved: CI output only
  - Verification method: `pixi install`; `cmake --preset test`; `cmake --build --preset test`; `ctest --preset test --output-on-failure`; `bash scripts/validate-installed-consumers.sh --preset test`; `cmake --preset quality`; `cmake --build --preset quality`
  - Dependencies: T2.3, T2.5, T2.6
  - Estimated complexity: M

## Phase 3: Consumer Switchover in Goggles

- [x] T3.1 Replace the tracked FC directory with a submodule at the same path and keep local override support
  - Description: Remove the tracked in-repo `filter-chain/` contents, add the extracted repository back as a git submodule at `filter-chain/`, and update the root `CMakeLists.txt` to use an overrideable `GOGGLES_FILTER_CHAIN_SOURCE_DIR` that still defaults to `${CMAKE_SOURCE_DIR}/filter-chain`.
  - Files involved: `filter-chain/` (gitlink), `.gitmodules`, `CMakeLists.txt`
  - Verification method: `git submodule status`; `test -f .gitmodules`; `git config --file .gitmodules --get submodule.filter-chain.path`; `git config --file .gitmodules --get submodule.filter-chain.url`; configure goggles with default path and with `-DGOGGLES_FILTER_CHAIN_SOURCE_DIR=/abs/path/to/local/checkout`
  - Dependencies: T2.7
  - Estimated complexity: M

- [x] T3.2 Update goggles CI checkout and remaining path-sensitive files for submodule-aware builds
  - Description: Change goggles CI jobs to initialize the `filter-chain/` submodule before any configure/build step, and update remaining path-sensitive files such as `.semgrepignore`, scripts, ROADMAP/docs, and any clean-checkout instructions that assumed FC was tracked directly in-repo.
  - Files involved: `.github/workflows/ci.yml`, `.semgrepignore`, `scripts/**/*`, `ROADMAP.md`, `docs/**/*`
  - Verification method: `rg -n "submodules: recursive|submodule update --init --recursive" .github/workflows/ci.yml`; `rg -n "filter-chain/" .semgrepignore scripts ROADMAP.md docs`; `git submodule update --init --recursive`
  - Dependencies: T3.1
  - Estimated complexity: M

- [x] T3.3 Remove or migrate goggles-owned FC verification that exceeds host integration scope
  - Description: Delete or migrate `tests/visual/test_intermediate_golden.cpp` and any other goggles-owned FC assertions that depend on intermediate outputs, public diagnostics-session lifecycle, or stable pass capture. Keep only goggles coverage that proves host wiring and end-to-end application behavior through the durable public FC boundary.
  - Files involved: `tests/visual/test_intermediate_golden.cpp`, `tests/visual/test_temporal_golden.cpp`, `tests/visual/runtime_capture.hpp`, `tests/visual/runtime_capture.cpp`, `tests/visual/CMakeLists.txt`
  - Verification method: `rg -n "intermediate_golden|capture_pass_output|diagnostic_session" tests/visual`; verify any remaining goggles FC-facing tests exercise host integration rather than pass-level ownership; `cmake --preset asan`; `cmake --build --preset asan --target test_temporal_golden`; `ctest --preset asan --output-on-failure --tests-regex "temporal_golden"`
  - Dependencies: T3.1, T3.2
  - Estimated complexity: M

- [x] T3.4 Run the goggles submodule integration gate from a clean checkout
  - Description: Validate goggles exactly as consumers and CI will see it: clean checkout, submodule init, configure/build/test, and full CI with FC coming only from the submodule and goggles retaining only host integration coverage.
  - Files involved: CI output only
  - Verification method: `git submodule update --init --recursive`; `pixi run ci --runner container --cache-mode warm --lane all`
  - Dependencies: T3.3
  - Estimated complexity: M

## Phase 4: Validation and Cleanup

- [x] T4.1 Tag `goggles-filter-chain` `v0.1.0` and repin goggles to the tagged release
  - Description: Create and push the standalone `v0.1.0` tag after both repositories are green, then update the goggles submodule pointer so the monorepo is pinned to the release tag instead of an arbitrary commit.
  - Files involved: standalone git tags, `filter-chain/` gitlink in goggles
  - Verification method: `git -C ../goggles-filter-chain tag --list`; `git -C ../goggles-filter-chain push --tags`; `git submodule status`; verify the submodule commit matches the tagged ref
  > **Completed**: Remote tag `v0.1.0` now resolves to `6e8d68922da5fca9c379b85debae3fe390ebea89`, and `goggles/filter-chain` is repinned to that tagged release state.
  - Dependencies: T3.4
  - Estimated complexity: S

- [x] T4.2 Remove stale transitional diagnostics/capture surface, wrappers, and docs across both repos
  - Description: Remove, internalize, or clearly de-support any remaining transitional public diagnostics-session lifecycle APIs, pass-capture affordances, wrappers, test seams, and documentation that were kept only to bridge pre-extraction goggles verification. For final convergence, retain `goggles_fc_chain_set_stage_mask`, `goggles_fc_chain_set_prechain_resolution`, `goggles_fc_chain_get_prechain_resolution`, `goggles_fc_chain_reset_control_value`, and `goggles_fc_chain_reset_all_controls` as the approved stable runtime-policy/reset subset, make `goggles/filter_chain.h` and `goggles/filter_chain.hpp` the canonical installed entrypoints, and remove `goggles_filter_chain.h` with no compatibility shim. The change is not complete until the stale transitional surface is gone from installed FC headers, goggles-owned tests/helpers, and both repositories' docs/spec-aligned guidance.
  - Files involved: `filter-chain/include/goggles/filter_chain.h`, `filter-chain/include/goggles/filter_chain.hpp`, `filter-chain/include/goggles_filter_chain.h`, `filter-chain/src/api/c_api.cpp`, `filter-chain/src/api/cpp_wrapper.cpp`, `filter-chain/src/runtime/chain.hpp`, `filter-chain/src/runtime/chain.cpp`, `tests/visual/runtime_capture.hpp`, `tests/visual/runtime_capture.cpp`, `README.md`, `CONTRIBUTING.md`, `openspec/changes/extract-filter-chain/tasks.md`
  - Verification method: `rg -n "diagnostic_session|capture_pass_output|goggles_fc_captured_image|public capture|intermediate pass capture" filter-chain/include filter-chain/src tests/visual README.md CONTRIBUTING.md openspec/changes/extract-filter-chain`; verify any remaining matches are explicitly internal/test-only and not installed public contract; rebuild FC contract tests and goggles host-integration tests with the final public surface only
  - Dependencies: T4.1
  - Estimated complexity: L

- [x] T4.3 Run final dual-repository release verification and prove cleanup completion before archive
  - Description: Re-run the standalone required checks and the goggles clean-checkout/full-CI flow after the release tag, submodule repin, and transitional-surface cleanup. Do not hand off to archive until both repos are green and verification proves no stale caller-facing migration surface remains.
  - Files involved: CI output only
  - Verification method: standalone required checks on `main`; `git submodule update --init --recursive`; `pixi run ci --runner container --cache-mode warm --lane all`; repeat the T4.2 stale-surface grep and confirm zero supported-surface matches remain
  - Dependencies: T4.2
  - Estimated complexity: M
