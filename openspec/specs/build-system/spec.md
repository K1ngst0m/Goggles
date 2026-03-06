# build-system Specification

## Purpose
This spec defines normative requirements for build system behavior in Goggles.
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

