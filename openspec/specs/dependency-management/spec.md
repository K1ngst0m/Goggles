# dependency-management Specification

## Purpose
Defines the dual-layer dependency management strategy using Pixi for system dependencies and toolchains, and CPM for C++ libraries.

## Requirements

### Requirement: Pixi as Primary Dependency Manager

The project SHALL use [Pixi](https://pixi.sh) as the primary dependency manager for system libraries, toolchains, and build tools.

#### Scenario: Fresh environment setup
- **GIVEN** a clean checkout of the repository
- **WHEN** `pixi install` is run
- **THEN** all system dependencies SHALL be installed from conda-forge
- **AND** the environment SHALL be reproducible via `pixi.lock`

#### Scenario: Pixi-managed dependencies
- **GIVEN** the `pixi.toml` configuration
- **THEN** the following categories SHALL be managed by Pixi:
  - Clang toolchain (clang, clang++, compiler-rt, libcxx)
  - Build tools (cmake, ninja, ccache)
  - System libraries (SDL3, CLI11, Vulkan headers)
  - X11/Wayland/audio libraries

### Requirement: CPM for C++ Libraries

The build system SHALL use CPM (CMake Package Manager) for C++ header-only and compiled libraries that are not available or suitable via Pixi.

#### Scenario: CPM-managed dependencies
- **GIVEN** the `cmake/Dependencies.cmake` file
- **WHEN** CMake configure is run
- **THEN** CPM SHALL download and cache C++ libraries including:
  - expected-lite (error handling)
  - spdlog (logging)
  - toml11 (configuration)
  - Catch2 (testing)
  - stb (image loading)
  - BS_thread_pool (concurrency)
  - slang (shader compiler)

#### Scenario: CPM cache reuse
- **GIVEN** a library is already in the CPM cache (`~/.cache/CPM`)
- **WHEN** CMake configure is run
- **THEN** CPM SHALL reuse the cached library without re-downloading

### Requirement: Pixi-CPM Integration

System libraries provided by Pixi SHALL be discovered by CMake using `find_package()` with `CPM_USE_LOCAL_PACKAGES=ON`.

#### Scenario: SDL3 discovery
- **GIVEN** SDL3 is installed via Pixi
- **WHEN** CMake processes `cmake/Dependencies.cmake`
- **THEN** `find_package(SDL3 REQUIRED)` SHALL locate the Pixi-provided SDL3
- **AND** CPM SHALL NOT attempt to download SDL3

#### Scenario: CLI11 discovery
- **GIVEN** CLI11 is installed via Pixi
- **WHEN** CMake processes `cmake/Dependencies.cmake`
- **THEN** `find_package(CLI11 REQUIRED)` SHALL locate the Pixi-provided CLI11

### Requirement: Dependency Version Pinning

All dependencies SHALL be pinned to specific versions to ensure reproducible builds.

#### Scenario: Pixi version constraints
- **GIVEN** the `pixi.toml` file
- **THEN** each dependency SHALL specify a version constraint (e.g., `>=3.2.28,<4`)
- **AND** exact versions SHALL be locked in `pixi.lock`

#### Scenario: CPM version pinning
- **GIVEN** the `cmake/Dependencies.cmake` file
- **WHEN** a library is added via CPM
- **THEN** a specific VERSION or GIT_TAG SHALL be specified

