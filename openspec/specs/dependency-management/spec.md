# dependency-management Specification

## Purpose
Defines the dual-layer dependency management strategy using Pixi for system dependencies, toolchains, and source-built/prebuilt packages. CPM is not used in the current configuration.
## Requirements
### Requirement: Pixi as Primary Dependency Manager

The project SHALL use [Pixi](https://pixi.sh) as the primary dependency manager for system libraries, toolchains, and build tools.

#### Scenario: Fresh environment setup
- **GIVEN** a clean checkout of the repository
- **WHEN** `pixi install` is run
- **THEN** all system dependencies SHALL be installed from conda-forge
- **AND** the environment SHALL be reproducible via `pixi.lock`

#### Scenario: Git worktree setup
- **GIVEN** an existing checkout with `pixi install` completed
- **WHEN** a new git worktree is created
- **THEN** `pixi install` SHALL complete without re-downloading source tarballs for conda-forge packages
- **AND** detached environments SHALL be shared via `~/.pixi/envs/` (when `detached-environments = true` in user config)

#### Scenario: Pixi-managed dependencies
- **GIVEN** the `pixi.toml` configuration
- **THEN** the following categories SHALL be managed by Pixi:
  - Clang toolchain (clang, clang++, compiler-rt, libcxx)
  - Build tools (cmake, ninja, ccache)
  - System libraries (SDL3, CLI11, Vulkan headers, Vulkan validation layers)
  - X11/Wayland/audio libraries

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

