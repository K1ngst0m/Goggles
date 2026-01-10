# dependency-management Specification

## Purpose
Defines the dual-layer dependency management strategy using Pixi for system dependencies, toolchains, and source-built/prebuilt packages. CPM is not used in the current configuration.
## Requirements
### Requirement: Pixi as Primary Dependency Manager

The project SHALL use Pixi as the enforced environment for builds and dependency resolution, anchored to a Glibc 2.28 baseline for cross-distro compatibility.

#### Scenario: Pixi environment enforcement
- **WHEN** CMake config runs for any target
- **THEN** it SHALL require `CONDA_PREFIX` to be set by Pixi
- **AND** it SHALL fail fast with guidance to run `pixi run build [preset]` if invoked outside Pixi

#### Scenario: Glibc 2.28 compatibility anchor
- **WHEN** dependencies are resolved via Pixi
- **THEN** `sysroot_linux-64` SHALL be pinned to the `2.28.*` series
- **AND** binaries built in the Pixi environment SHALL be compatible with RHEL8/Ubuntu 20.04 class distros

### Requirement: Pixi for C++ Libraries (Primary)

The build system SHALL consume C++ libraries from Pixi packages (including source-built recipes) as the primary source.

#### Scenario: Pixi-managed C++ libraries
- **GIVEN** the `pixi.toml` configuration
- **THEN** the following libraries SHALL be provided by Pixi packages (built from source where applicable):
  - expected-lite (error handling)
  - spdlog (logging)
  - toml11 (configuration)
  - Catch2 (testing)
  - stb (image loading)
  - BS_thread_pool (concurrency)
  - slang-shaders (Slang shader compiler) - intentionally managed as local package for independent version control
  - Tracy (profiling, optional)

#### Scenario: Pixi package discovery
- **GIVEN** `CPM_USE_LOCAL_PACKAGES=ON` is set during CMake configure
- **WHEN** `find_package()` is invoked for the above libraries
- **THEN** the Pixi-provided packages SHALL be found without CPM downloads
- **NOTE**: Slang shader compiler is intentionally managed as a local pixi-build package for independent version control
- **RATIONALE**: This allows the project to control Slang updates independently from conda-forge package updates

### Requirement: Pixi-CPM Integration

System libraries provided by Pixi SHALL be discovered by CMake using `find_package()` without CPM downloads.

#### Scenario: SDL3 discovery
- **GIVEN** SDL3 is installed via Pixi
- **WHEN** CMake processes `cmake/Dependencies.cmake`
- **THEN** `find_package(SDL3 REQUIRED)` SHALL locate the Pixi-provided SDL3
- **AND** CPM SHALL NOT be used

#### Scenario: CLI11 discovery
- **GIVEN** CLI11 is installed via Pixi
- **WHEN** CMake processes `cmake/Dependencies.cmake`
- **THEN** `find_package(CLI11 REQUIRED)` SHALL locate the Pixi-provided CLI11
- **AND** CPM SHALL NOT be used

### Requirement: Dependency Version Pinning

Dependency resolution SHALL be anchored by the sysroot version while allowing Pixi to solve other packages, with exact versions locked in `pixi.lock`.

#### Scenario: Sysroot version constraint
- **GIVEN** `pixi.toml`
- **WHEN** dependencies are installed
- **THEN** `sysroot_linux-64` SHALL declare version constraint `2.28.*`
- **AND** 32-bit sysroot builds SHALL consume the matching baseline

#### Scenario: Solver-driven versions with lockfile
- **WHEN** Pixi installs dependencies with wildcard constraints
- **THEN** exact resolved versions SHALL be captured in `pixi.lock`
- **AND** subsequent installs SHALL reproduce those versions from the lockfile

### Requirement: Worktree-Compatible Hook Installation

The pre-commit hook installation SHALL work correctly in git worktrees.

#### Scenario: Hook installation in worktree
- **GIVEN** a git worktree created from the main repository
- **WHEN** `pixi run init` is executed
- **THEN** the pre-commit hook SHALL be installed to the correct hooks directory
- **AND** the script SHALL use `git rev-parse --git-path hooks` to locate the hooks directory

#### Scenario: Hook installation with custom hooksPath
- **GIVEN** `core.hooksPath` is configured in git config
- **WHEN** `pixi run init` is executed
- **THEN** the pre-commit hook SHALL be installed to the configured hooksPath

### Requirement: Vulkan Components from conda-forge

Vulkan headers and validation layers SHALL be sourced from conda-forge packages instead of local pixi-build packages.

#### Scenario: Vulkan package availability
- **GIVEN** the `pixi.toml` configuration
- **THEN** the following Vulkan components SHALL be provided by conda-forge:
  - `libvulkan-headers = "1.4.328.*"` - Vulkan API headers
  - `vulkan-validation-layers = "1.4.328.*"` - Debug validation layers for development

#### Scenario: Validation layer activation
- **GIVEN** the pixi environment is activated
- **WHEN** a Vulkan application runs with validation enabled
- **THEN** `VK_ADD_LAYER_PATH` SHALL point to `$CONDA_PREFIX/share/vulkan/explicit_layer.d`
- **AND** `VULKAN_SDK` SHALL be set to `$CONDA_PREFIX`

#### Rationale
- The project uses Slang for shader compilation (via `slang-shaders` local package)
- glslang, shaderc, and spirv-cross are not required dependencies
- Only validation layers are needed for development and debugging

### Requirement: Sysroot Package Integrity

Sysroot packages SHALL verify upstream artifacts and self-heal known packaging issues before use.

#### Scenario: SHA256 verification for upstream debs
- **WHEN** the 32-bit sysroot recipe downloads Debian/Ubuntu archives
- **THEN** each archive SHALL be validated against an expected SHA256
- **AND** the build SHALL fail if any checksum mismatches

#### Scenario: Symlink self-repair
- **WHEN** GCC development libraries in the sysroot include broken absolute symlinks
- **THEN** the recipe SHALL repoint them to local targets or remove unusable links to avoid linker errors

#### Scenario: Tracy source integrity
- **WHEN** Tracy sources are fetched for the sysroot build
- **THEN** the recipe SHALL verify the commit hash matches the expected revision before building

