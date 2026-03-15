# build-system Specification

## Purpose
TBD - created by archiving change add-version-management. Update Purpose after archive.
## Requirements
### Requirement: Single Source of Truth for Version

The build system SHALL maintain project version in a single authoritative location that automatically propagates to all code via compile definitions.

#### Scenario: Version defined in CMake project directive

- **GIVEN** the root `CMakeLists.txt` file
- **WHEN** the `project()` directive is invoked
- **THEN** the VERSION parameter SHALL specify the project version in `MAJOR.MINOR.PATCH` format
- **AND** this SHALL be the only location where version is manually maintained

#### Scenario: CMake version variables available after project directive

- **GIVEN** `project(goggles VERSION 0.1.0)` has been invoked
- **WHEN** CMake configuration proceeds
- **THEN** variables `PROJECT_VERSION`, `PROJECT_VERSION_MAJOR`, `PROJECT_VERSION_MINOR`, `PROJECT_VERSION_PATCH` SHALL be defined
- **AND** variable `PROJECT_NAME` SHALL contain the project name

### Requirement: Version Component Compile Definitions

The build system SHALL define preprocessor macros for individual version components.

#### Scenario: Major minor patch macros defined

- **GIVEN** project version is `0.1.0`
- **WHEN** CMake processes compile definitions
- **THEN** `GOGGLES_VERSION_MAJOR` SHALL be defined as `0`
- **AND** `GOGGLES_VERSION_MINOR` SHALL be defined as `1`
- **AND** `GOGGLES_VERSION_PATCH` SHALL be defined as `0`
- **AND** these SHALL be defined via `add_compile_definitions()`

#### Scenario: Vulkan version compatibility

- **GIVEN** `GOGGLES_VERSION_MAJOR`, `GOGGLES_VERSION_MINOR`, `GOGGLES_VERSION_PATCH` macros are defined
- **WHEN** Vulkan application info is populated
- **THEN** `VK_MAKE_VERSION(GOGGLES_VERSION_MAJOR, GOGGLES_VERSION_MINOR, GOGGLES_VERSION_PATCH)` SHALL compile without errors
- **AND** SHALL produce correct Vulkan version encoding

### Requirement: No Hardcoded Version Values

Source code SHALL NOT contain hardcoded version numbers independent of CMake project version.

#### Scenario: Vulkan backend uses version macros

- **GIVEN** `VulkanBackend::create_instance()` sets Vulkan application info
- **WHEN** `applicationVersion` and `engineVersion` are assigned
- **THEN** they SHALL use `VK_MAKE_VERSION(GOGGLES_VERSION_MAJOR, GOGGLES_VERSION_MINOR, GOGGLES_VERSION_PATCH)`
- **AND** SHALL NOT use hardcoded values like `VK_MAKE_VERSION(0, 1, 0)`

#### Scenario: No hardcoded version strings in source

- **GIVEN** the version management system is implemented
- **WHEN** searching source files with `rg "0\.1\.0" src/`
- **THEN** no hardcoded version strings SHALL be found in source files
- **AND** all version references SHALL use compile definition macros

### Requirement: Version Change Propagation

Changes to the project version SHALL automatically propagate to all code without manual updates.

#### Scenario: Version update workflow

- **GIVEN** project version is `0.1.0` and code uses `GOGGLES_VERSION_*` macros
- **WHEN** `project(goggles VERSION 0.2.0)` is modified in `CMakeLists.txt`
- **AND** rebuild is performed
- **THEN** all macros SHALL reflect version `0.2.0` after recompilation
- **AND** no manual code changes SHALL be required
- **AND** Vulkan application info SHALL show version `0.2.0`

### Requirement: Toolchain Version Pinning

The build system SHALL pin all development tool versions in pixi.toml to prevent system tool leakage and ensure reproducible builds.

#### Scenario: Clang toolchain version consistency
- **WHEN** building with pixi
- **THEN** clang, clang++, lld, and clang-tools SHALL use the same major version (21.x)

#### Scenario: Build tool version pinning
- **WHEN** pixi.toml specifies cmake, ninja, ccache
- **THEN** each tool SHALL have a pinned version constraint (not `*`)

#### Scenario: Format tool version consistency
- **WHEN** running format tasks in default or lint environment
- **THEN** taplo version SHALL be identical across environments

### Requirement: Visual test targets build unconditionally
The build system SHALL build all visual test clients and the image comparison library as part of the default build, since all dependencies (wayland-client, wayland-protocols, stb_image, Catch2) are already project requirements.

#### Scenario: Default build includes visual targets
- **GIVEN** a clean CMake configuration using any preset
- **WHEN** the build completes
- **THEN** all test client binaries (`solid_color_client`, `gradient_client`, `quadrant_client`, `multi_surface_client`) SHALL be built
- **AND** `goggles_image_compare` CLI binary SHALL be built
- **AND** the `image_compare` static library SHALL be built
- **AND** `test_image_compare` Catch2 test binary SHALL be built

### Requirement: CTest label taxonomy
The build system SHALL register test targets under a consistent label taxonomy using `set_tests_properties(... LABELS ...)`.

#### Scenario: Unit label unchanged
- **WHEN** `ctest -L unit` is run with any preset
- **THEN** existing Catch2 unit tests and `image_compare_unit_tests` SHALL run
- **AND** no integration tests SHALL be included

#### Scenario: Integration label includes headless smoke
- **WHEN** `ctest -L integration` is run with any preset
- **THEN** the headless pipeline smoke test (`headless_smoke`, `headless_smoke_png_check`) SHALL be included
- **AND** existing integration tests (e.g., `auto_input_forwarding`, when available) SHALL also be included

#### Scenario: Visual label for visual regression tests
- **WHEN** `ctest -L visual` is run with any preset
- **THEN** only visual regression test targets (Phase 2+) SHALL run
- **AND** unit and integration tests SHALL NOT be included unless also labeled `visual`

### Requirement: Deterministic Semgrep Tooling

The build system SHALL provide a Pixi-managed Semgrep toolchain and checked-in rule source so local and CI static analysis use the same deterministic inputs.

#### Scenario: Pixi provides Semgrep for local and CI execution
- **GIVEN** the repository defines lint and developer workflow tooling in `pixi.toml`
- **WHEN** contributors or CI invoke the Semgrep entrypoint
- **THEN** the Semgrep binary SHALL come from the repository-managed Pixi environment
- **AND** the same Semgrep version surface SHALL be used locally and in CI

#### Scenario: Semgrep provenance is observable during verification
- **GIVEN** the repository verifies the Semgrep tool surface before enforcing the gate
- **WHEN** maintainers inspect the Semgrep path and version under the repository-managed workflow
- **THEN** the resolved Semgrep executable SHALL originate from the Pixi-managed environment
- **AND** the reported version SHALL match the locked local and CI Semgrep surface

#### Scenario: Pixi source-of-truth files stay synchronized
- **GIVEN** the repository adds Semgrep to the Pixi-managed tool surface
- **WHEN** the change updates Semgrep dependency configuration
- **THEN** `pixi.toml` SHALL declare the dependency version surface
- **AND** `pixi.lock` SHALL be updated in sync with that change

#### Scenario: Dependency admission remains reviewable
- **GIVEN** the repository adds Semgrep as a new dependency for the static-analysis workflow
- **WHEN** the proposal and apply artifacts are reviewed
- **THEN** they SHALL include dependency rationale, license compatibility review, maintenance assessment, and team agreement evidence
- **AND** the dependency SHALL NOT be treated as implicitly admitted just because the tool is easy to install

#### Scenario: Initial scan roots stay limited to repository-managed C and C++ code
- **GIVEN** the repository enables Semgrep policy checks
- **WHEN** the `pixi run semgrep` entrypoint runs in its initial configuration
- **THEN** it SHALL scan repository-managed code under `src/` and `tests/`
- **AND** it SHALL use narrower path filters for rules that apply only to selected subsystems

#### Scenario: Semgrep rule sources are checked into the repository
- **GIVEN** the repository enables Semgrep policy checks
- **WHEN** the Semgrep entrypoint runs
- **THEN** it SHALL load configuration and rules from checked-in repository files
- **AND** it SHALL NOT depend on registry defaults or hosted rule configuration

#### Scenario: Subsystem-sensitive rules stay path-scoped
- **GIVEN** some policy bans only apply to selected Goggles subsystems
- **WHEN** the repository defines Semgrep rules for Vulkan API split or render-path threading
- **THEN** those rules SHALL scope to the directories where the policy applies
- **AND** they SHALL exclude directories with explicit policy exceptions such as `src/capture/vk_layer/`

### Requirement: CMake-First Standalone Filter Project Workflow

The build system SHALL define the extracted filter runtime as a CMake-first standalone project with
repository layout rooted at `include/`, `src/`, `tests/`, `assets/`, and `cmake/`. The documented
configure, build, test, install, and export workflow SHALL be runnable from a clean checkout without
requiring Goggles-specific wrappers.

#### Scenario: Clean checkout uses standalone CMake entry points
- **GIVEN** a clean checkout of the extracted filter-chain project
- **WHEN** a maintainer follows the documented standalone workflow
- **THEN** configure, build, test, and install steps SHALL execute through project-owned CMake entry points
- **AND** the workflow SHALL NOT require Pixi task wrappers, Goggles preset files, or Conda-specific environment assumptions

#### Scenario: Separate consumer validates exported package
- **GIVEN** the standalone project has been installed to a prefix
- **WHEN** a separate CMake consumer project resolves that install tree
- **THEN** package discovery, target resolution, and public-header inclusion SHALL succeed through the exported package contract
- **AND** validation SHALL occur without adding the library sources back into the consumer source tree

#### Scenario: Package config template generates valid export

- **GIVEN** the standalone project is installed to a prefix
- **WHEN** a downstream consumer calls `find_package(GogglesFilterChain CONFIG REQUIRED)`
- **THEN** the generated config SHALL define `GogglesFilterChain::goggles-filter-chain` as an
  imported target
- **AND** the config SHALL resolve transitive PUBLIC dependencies before defining the target

### Requirement: Goggles In-Tree Subdirectory Primary Path

Goggles SHALL consume the in-repo filter runtime through `add_subdirectory(filter-chain)` as the
primary and default integration path. The standalone build and `find_package(...)` path SHALL remain
available for validating the exported package contract (installed consumer tests) but SHALL NOT be
required for normal Goggles builds.

#### Scenario: Goggles normal build uses subdirectory inclusion
- **GIVEN** the filter-chain source tree is present under `filter-chain/` in the Goggles repo
- **WHEN** Goggles is configured with any build preset
- **THEN** the build SHALL include the filter runtime via `add_subdirectory(filter-chain)`
- **AND** no separate pre-build, install, or `CMAKE_PREFIX_PATH` step SHALL be required

#### Scenario: Installed package validation remains available
- **GIVEN** the standalone filter-chain project can be configured, built, and installed independently
- **WHEN** maintainers run installed-consumer validation (e.g. `validate-installed-consumers.sh`)
- **THEN** the validation script SHALL perform its own standalone build and install
- **AND** the exported `find_package(GogglesFilterChain CONFIG REQUIRED)` contract SHALL continue to work for downstream consumers outside the Goggles repo

### Requirement: Paired Static and Shared Package Outputs

The standalone build and export workflow SHALL publish supported `STATIC` and `SHARED` library
outputs for the filter runtime. The exported package contract SHALL validate both output forms and
SHALL NOT require a `MODULE` target.

#### Scenario: Package exports static and shared variants
- **GIVEN** the standalone project is built for distribution
- **WHEN** install and export artifacts are inspected
- **THEN** supported package artifacts SHALL include both `STATIC` and `SHARED` library outputs
- **AND** consumers SHALL not be required to build or load a `MODULE` target to use the package

#### Scenario: Downstream validation covers both supported output forms
- **GIVEN** downstream consumer validation is run against the installed standalone package
- **WHEN** maintainers verify supported linkage modes
- **THEN** the validation evidence SHALL cover consumption of both `STATIC` and `SHARED` outputs
- **AND** success criteria SHALL not treat a `MODULE` build as an acceptable substitute for either supported output form

### Requirement: Transitional Preset Cleanup After Package Validation

The build system SHALL remove the transitional `.shared` and `test-shared` CMake presets from
`CMakePresets.json` once the standalone project's package export proves both STATIC and SHARED
consumption paths. These presets SHALL NOT remain in the preset file after successful package
validation.

#### Scenario: Transitional presets removed after both linkage modes proven

- **GIVEN** the standalone project has been installed and downstream consumer validation has
  proven both STATIC and SHARED linkage through `find_package(GogglesFilterChain)`
- **WHEN** maintainers inspect `CMakePresets.json`
- **THEN** the `.shared` and `test-shared` presets SHALL have been removed
- **AND** existing named presets (`debug`, `release`, `asan`, `quality`, `test`) SHALL remain unchanged

#### Scenario: Presets not removed prematurely

- **GIVEN** the standalone project has NOT yet completed consumer validation for both linkage modes
- **WHEN** maintainers inspect `CMakePresets.json`
- **THEN** the `.shared` and `test-shared` presets SHALL still be present
- **AND** those presets SHALL continue to function for transitional verification

### Requirement: Standalone Dependency Discovery Module

The standalone project SHALL provide a `cmake/FilterChainDependencies.cmake` module that
encapsulates third-party dependency discovery for all required external libraries. The module
SHALL use standard `find_package()` calls and SHALL NOT require Goggles-specific Find modules
or Pixi/Conda-specific path assumptions.

#### Scenario: Dependency module discovers all required third-party libraries

- **GIVEN** the standalone project is configured from a clean checkout with dependencies available
  through standard CMake package discovery
- **WHEN** `cmake/FilterChainDependencies.cmake` is included during configuration
- **THEN** the module SHALL resolve Vulkan, expected-lite, spdlog, slang, stb_image, and Catch2
  through standard `find_package()` calls
- **AND** configuration SHALL NOT require Goggles-owned CMake Find modules

#### Scenario: Dependency module is reusable by exported package config

- **GIVEN** the standalone project has been installed and a downstream consumer resolves the package
- **WHEN** the exported `GogglesFilterChainConfig.cmake` is loaded by the consumer
- **THEN** the package config SHALL invoke dependency discovery for transitive PUBLIC dependencies
  (Vulkan, expected-lite)
- **AND** PRIVATE dependencies (spdlog, slang, stb_image) SHALL NOT leak to the consumer

### Requirement: Downstream Consumer Validation Projects

The standalone project SHALL include out-of-tree consumer validation projects that prove the
installed package is discoverable and linkable for both STATIC and SHARED library outputs. These
validation projects SHALL use only `find_package(GogglesFilterChain CONFIG REQUIRED)` and the
installed public surface.

#### Scenario: Static consumer validation succeeds

- **GIVEN** the standalone project has been installed to a prefix with STATIC library output
- **WHEN** a consumer validation project configures with `CMAKE_PREFIX_PATH` pointing to the
  install prefix
- **THEN** `find_package(GogglesFilterChain CONFIG REQUIRED)` SHALL succeed
- **AND** the consumer SHALL compile and link against the STATIC target without errors

#### Scenario: Shared consumer validation succeeds

- **GIVEN** the standalone project has been installed to a prefix with SHARED library output
- **WHEN** a consumer validation project configures with `CMAKE_PREFIX_PATH` pointing to the
  install prefix
- **THEN** `find_package(GogglesFilterChain CONFIG REQUIRED)` SHALL succeed
- **AND** the consumer SHALL compile and link against the SHARED target without errors

#### Scenario: Consumer validation uses only installed surface

- **GIVEN** a consumer validation project
- **WHEN** its source files are inspected
- **THEN** it SHALL include only headers from the installed package include root
- **AND** it SHALL NOT reference standalone project source-tree paths or Goggles-owned headers

### Requirement: Host Test Split After Extraction

After contract tests are moved to the standalone project, the Goggles test suite SHALL retain
only host integration tests that exercise the wiring between Goggles backend subsystems and the
filter-chain boundary. The Goggles test target SHALL NOT duplicate contract tests that are
owned and run by the standalone project.

#### Scenario: Goggles retains host integration tests

- **GIVEN** the standalone project owns ~22 contract test files
- **WHEN** the Goggles `tests/CMakeLists.txt` is inspected
- **THEN** it SHALL include host integration tests (e.g., `test_filter_boundary_contracts.cpp`,
  `test_vulkan_backend_subsystem_contracts.cpp`, `test_filter_chain_retarget.cpp`)
- **AND** those tests SHALL compile against the installed or subdirectory-provided filter-chain target

#### Scenario: Moved contract tests are absent from Goggles

- **GIVEN** ~22 contract test files have been moved to `filter-chain/tests/`
- **WHEN** the Goggles `tests/` directory is inspected
- **THEN** the moved contract test source files SHALL NOT be present in the Goggles test directory
- **AND** the Goggles test CMakeLists SHALL NOT reference the moved source files
