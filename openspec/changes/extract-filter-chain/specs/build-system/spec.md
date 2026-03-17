# Delta for build-system

## MODIFIED Requirements

### Requirement: Standalone Dependency Discovery Module

(Previously: `FilterChainDependencies.cmake` MAY have used `$ENV{CONDA_PREFIX}` hints for dependency discovery.)

The standalone project SHALL provide a `cmake/FilterChainDependencies.cmake` module that uses exclusively standard `find_package()` calls for all third-party dependency discovery. The module SHALL NOT reference `$ENV{CONDA_PREFIX}`, `$ENV{CONDA_BUILD_SYSROOT}`, or any Conda/Pixi-specific environment variables for path hints. Dependencies SHALL be discoverable through standard CMake search paths (`CMAKE_PREFIX_PATH`, system paths, or package-specific `_DIR` variables).

#### Scenario: No Conda environment variable references

- **GIVEN** the file `filter-chain/cmake/FilterChainDependencies.cmake`
- **WHEN** a text search for `CONDA_PREFIX`, `CONDA_BUILD_SYSROOT`, or `ENV{CONDA` is executed
- **THEN** zero matches SHALL be found
- **AND** all dependency discovery SHALL use standard CMake `find_package()` mechanisms

#### Scenario: Standalone build without Pixi wrapper

- **GIVEN** a clean checkout of the filter-chain project with dependencies available on standard CMake search paths
- **WHEN** `cmake -S . -B build` is executed without any Pixi/Conda environment active
- **THEN** configuration SHALL succeed by discovering all dependencies through standard `find_package()` calls
- **AND** no configuration step SHALL require `$ENV{CONDA_PREFIX}` to be set

#### Scenario: Dependency module resolves all required libraries

- **GIVEN** the standalone project is configured from a clean checkout
- **WHEN** `cmake/FilterChainDependencies.cmake` is included during configuration
- **THEN** the module SHALL resolve Vulkan, expected-lite, spdlog, slang, stb_image, and Catch2 through standard `find_package()` calls
- **AND** configuration SHALL NOT require Goggles-owned CMake Find modules

### Requirement: In-Repo Subdirectory Bridge During Extraction

(Previously: Specified `add_subdirectory(filter-chain/)` as transitional bridge before `find_package()` switch.)

**Replaced by**: Submodule Integration (see ADDED section below). The filter-chain directory is replaced by a git submodule. The `add_subdirectory()` integration pattern is preserved but now targets the submodule path instead of an in-repo directory.

## ADDED Requirements

### Requirement: Cross-Repository Verification Ownership Split

After extraction, build and test ownership SHALL be split by boundary responsibility. Standalone `filter-chain` SHALL own intermediate-pass golden tests and other diagnostics-heavy verification that depends on internal capture-oriented capabilities. Goggles SHALL retain end-to-end host integration coverage only.

#### Scenario: Standalone project owns intermediate-pass verification

- **GIVEN** verification assets that assert on intermediate pass outputs or diagnostics-heavy golden baselines
- **WHEN** test ownership is reviewed after extraction
- **THEN** those assets SHALL live in standalone `filter-chain` build and test targets
- **AND** they SHALL NOT remain Goggles-owned requirements for normal host builds

#### Scenario: Goggles build keeps host integration coverage

- **GIVEN** the Goggles repository after extraction
- **WHEN** Goggles FC-facing test targets are inspected
- **THEN** they SHALL cover host wiring and end-to-end integration behavior against the public boundary
- **AND** they SHALL NOT duplicate standalone `filter-chain` intermediate-pass golden coverage

### Requirement: Migration Cleanup Completes the Build/Test Transition

The extraction build/test transition SHALL NOT be considered complete until stale transitional build wiring, test wiring, and supported-surface assumptions have been cleaned up after migration. Temporary accommodations that keep deprecated caller-facing diagnostics-session or pass-capture behavior alive for Goggles-owned verification SHALL be removed, internalized, or reassigned once the new ownership split is in place.

#### Scenario: Transitional Goggles-owned capture wiring is removed

- **GIVEN** standalone `filter-chain` owns the intermediate and diagnostics-heavy verification flows
- **WHEN** Goggles build and test wiring is inspected at change completion
- **THEN** Goggles-owned targets SHALL no longer depend on transitional public pass-capture or diagnostics-session lifecycle accommodations
- **AND** stale transitional test/build wiring SHALL not remain as part of the supported extracted boundary

### Requirement: Standalone Build Independence

The filter-chain project SHALL build independently as a complete CMake project from a clean checkout without requiring the goggles parent repository context, Pixi task wrappers, or Conda-specific environment assumptions.

#### Scenario: Standalone configure and build

- **GIVEN** a clean checkout of the filter-chain standalone repository
- **WHEN** `cmake -S . -B build` and `cmake --build build` are executed with dependencies on standard search paths
- **THEN** configuration and build SHALL succeed
- **AND** no file from the goggles parent repository SHALL be required

#### Scenario: Standalone test execution

- **GIVEN** a successfully built standalone filter-chain project
- **WHEN** `ctest --test-dir build` is executed
- **THEN** all contract tests SHALL pass
- **AND** test execution SHALL NOT depend on goggles repository test fixtures or infrastructure

#### Scenario: Standalone install and consume

- **GIVEN** a successfully built standalone filter-chain project
- **WHEN** the project is installed to a prefix via `cmake --install build --prefix /tmp/fc-install`
- **AND** a consumer validation project configures with `CMAKE_PREFIX_PATH=/tmp/fc-install`
- **THEN** `find_package(GogglesFilterChain CONFIG REQUIRED)` SHALL succeed
- **AND** both STATIC and SHARED consumer validation projects SHALL compile and link without errors

### Requirement: Standalone Pixi Environment

The standalone filter-chain repository SHALL have its own `pixi.toml` for dependency management, independent of the goggles monorepo `pixi.toml`.

#### Scenario: Own pixi.toml with FC-specific dependencies

- **GIVEN** the standalone filter-chain repository root
- **WHEN** `pixi.toml` is inspected
- **THEN** it SHALL declare FC-specific dependencies: Vulkan, spdlog, slang, expected-lite, stb, Catch2, and clang-tools
- **AND** it SHALL NOT reference goggles-specific dependencies (SDL3, CLI11, toml11, BS_thread_pool, wayland-client)

#### Scenario: Pixi environment builds the project

- **GIVEN** the standalone filter-chain repository with its own `pixi.toml`
- **WHEN** `pixi install` and the documented pixi build task are executed
- **THEN** all dependencies SHALL be resolved from the standalone `pixi.toml`
- **AND** the project SHALL build successfully within the pixi environment

### Requirement: Standalone CMake Presets

The standalone filter-chain repository SHALL have its own `CMakePresets.json` providing build presets appropriate for the library project.

#### Scenario: CMakePresets.json contains required presets

- **GIVEN** the standalone filter-chain repository root
- **WHEN** `CMakePresets.json` is inspected
- **THEN** it SHALL contain at minimum: debug, release, asan, quality, and test presets
- **AND** presets SHALL be self-contained (no `include` of goggles preset files)

#### Scenario: Presets work for standalone builds

- **GIVEN** the standalone filter-chain `CMakePresets.json`
- **WHEN** `cmake --preset test` is executed
- **THEN** configuration SHALL succeed using only the standalone preset definitions
- **AND** the resulting build directory SHALL be independent of any goggles build artifacts

### Requirement: Submodule Integration for Goggles

The goggles monorepo SHALL consume the extracted filter-chain via git submodule, preserving `add_subdirectory()` build semantics. The submodule SHALL be pinned to a specific commit or tag for deterministic builds.

#### Scenario: Goggles build with submodule

- **GIVEN** the goggles repository with filter-chain configured as a git submodule
- **WHEN** `git submodule update --init` is executed followed by a full build
- **THEN** the build SHALL succeed with `add_subdirectory()` consuming the submodule path
- **AND** all goggles tests SHALL pass

#### Scenario: Submodule pinned to specific ref

- **GIVEN** the `.gitmodules` file in the goggles repository
- **WHEN** the submodule configuration is inspected
- **THEN** the filter-chain submodule SHALL reference `git@github.com:goggles-dev/goggles-filter-chain.git`
- **AND** the submodule working copy SHALL be pinned to a specific commit (tag preferred)

#### Scenario: Goggles CI passes with submodule integration

- **GIVEN** the goggles repository with the filter-chain submodule
- **WHEN** `pixi run ci --runner container --cache-mode warm --lane all` is executed
- **THEN** all CI lanes SHALL pass
- **AND** the filter-chain submodule SHALL be initialized and available during the build

#### Scenario: Local development override documented

- **GIVEN** a developer working on both goggles and filter-chain simultaneously
- **WHEN** they need to test local filter-chain changes without pushing to the submodule remote
- **THEN** the documented workflow SHALL describe how to use a local path override (e.g., `add_subdirectory()` with a local checkout path via CMake variable)
- **AND** the override mechanism SHALL NOT require modifying `.gitmodules` or committed CMake files

### Requirement: Filter-Chain Directory Removal

After submodule integration, the `filter-chain/` in-repo directory SHALL be removed from the goggles repository and replaced entirely by the git submodule.

#### Scenario: No in-repo filter-chain source after extraction

- **GIVEN** the goggles repository after extraction is complete
- **WHEN** the repository contents are inspected (excluding the submodule)
- **THEN** no `filter-chain/` directory with FC source code SHALL exist as tracked repository content
- **AND** the path SHALL be occupied by the git submodule reference only

#### Scenario: Git history preserved in standalone repo

- **GIVEN** the standalone filter-chain repository created via `git filter-repo --subdirectory-filter filter-chain/`
- **WHEN** the git log is inspected
- **THEN** the commit history relevant to filter-chain files SHALL be preserved
- **AND** the history SHALL be clean (no goggles-only commits)
