# dependency-management Spec Delta

## Purpose
Capture build-system hardening driven by Pixi: enforced Pixi environments, Glibc 2.28 baseline, and integrity protections for sysroot assets.

## Requirements

## MODIFIED Requirements

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

## ADDED Requirements

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
