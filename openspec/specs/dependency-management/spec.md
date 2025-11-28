# dependency-management Specification

## Purpose
TBD - created by archiving change add-sdl3-cpm. Update Purpose after archive.
## Requirements
### Requirement: SDL3 Dependency via CPM
The build system SHALL provide SDL3 as a CPM-managed dependency, fetched automatically during CMake configuration.

#### Scenario: Fresh build configuration
- **GIVEN** a clean checkout of the repository
- **WHEN** CMake configure is run
- **THEN** CPM SHALL download and build SDL3 from source

#### Scenario: Cached build
- **GIVEN** SDL3 is already in the CPM cache (`~/.cache/CPM`)
- **WHEN** CMake configure is run
- **THEN** CPM SHALL reuse the cached SDL3 without re-downloading

### Requirement: SDL3 Version Pinning
The build system SHALL pin SDL3 to a specific version to ensure reproducible builds.

#### Scenario: Version specification
- **GIVEN** the `cmake/Dependencies.cmake` file
- **WHEN** SDL3 is added via CPM
- **THEN** a specific Git tag or version number SHALL be specified

