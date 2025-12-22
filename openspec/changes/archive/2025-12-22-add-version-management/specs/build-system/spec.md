## ADDED Requirements

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
