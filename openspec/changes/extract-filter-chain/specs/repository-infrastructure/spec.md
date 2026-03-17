# repository-infrastructure Specification

## Purpose

Define the standalone repository infrastructure requirements for the extracted filter-chain project at `goggles-dev/goggles-filter-chain`, covering licensing, versioning, branch protection, and repository configuration.

## Requirements

### Requirement: MIT License

The standalone filter-chain repository SHALL be published under the MIT license, consistent with the goggles parent project license.

#### Scenario: License file present and correct

- **GIVEN** the standalone filter-chain repository root
- **WHEN** the `LICENSE` file is inspected
- **THEN** it SHALL contain a valid MIT license text
- **AND** the copyright holder and year SHALL be consistent with the goggles project license

#### Scenario: License discoverable by package managers

- **GIVEN** the standalone repository metadata (README, CMakeLists.txt)
- **WHEN** license information is queried
- **THEN** the project SHALL identify its license as MIT
- **AND** the `LICENSE` file SHALL be present at the repository root

### Requirement: Independent Semantic Versioning

The standalone filter-chain project SHALL maintain its own independent semantic version, starting at 0.1.0. The version SHALL NOT be coupled to or derived from the goggles project version.

#### Scenario: Initial version is 0.1.0

- **GIVEN** the standalone filter-chain `CMakeLists.txt`
- **WHEN** the `project()` directive is inspected
- **THEN** the VERSION parameter SHALL be `0.1.0`
- **AND** the project name SHALL be `GogglesFilterChain`

#### Scenario: Version is independent of goggles

- **GIVEN** the goggles project at version X.Y.Z
- **WHEN** the standalone filter-chain project version is inspected
- **THEN** the filter-chain version SHALL be independently managed
- **AND** version bumps in either project SHALL NOT require corresponding bumps in the other

#### Scenario: Version tag at extraction completion

- **GIVEN** the standalone filter-chain repository with all CI lanes passing
- **WHEN** extraction is complete and validated
- **THEN** a git tag `v0.1.0` SHALL be created on the main branch
- **AND** the goggles submodule SHALL be pinned to this tagged release

### Requirement: GitHub Repository Configuration

The standalone filter-chain repository SHALL be configured as a public GitHub repository with appropriate branch protection and issue tracking.

#### Scenario: Repository is public

- **GIVEN** the repository at `github.com/goggles-dev/goggles-filter-chain`
- **WHEN** repository visibility is inspected
- **THEN** the repository SHALL be publicly accessible
- **AND** external consumers SHALL be able to clone without authentication

#### Scenario: Main branch protection enabled

- **GIVEN** the standalone filter-chain repository
- **WHEN** branch protection rules for `main` are inspected
- **THEN** direct pushes to `main` SHALL be prohibited
- **AND** pull request reviews SHALL be required before merging
- **AND** CI status checks SHALL be required to pass before merging

#### Scenario: Issue tracker enabled

- **GIVEN** the standalone filter-chain repository
- **WHEN** the repository settings are inspected
- **THEN** the GitHub issue tracker SHALL be enabled
- **AND** external users SHALL be able to file issues

### Requirement: Repository Documentation

The standalone filter-chain repository SHALL include a README.md with essential project documentation for external consumers.

#### Scenario: README contains build instructions

- **GIVEN** the standalone filter-chain repository README.md
- **WHEN** a new user reads the documentation
- **THEN** the README SHALL document how to build the project from source
- **AND** it SHALL document how to run tests
- **AND** it SHALL document how to consume the library as a CMake dependency

#### Scenario: README documents local development override

- **GIVEN** the standalone filter-chain README.md
- **WHEN** a developer working on both goggles and filter-chain reads the documentation
- **THEN** the README SHALL describe the local path override mechanism for co-development
- **AND** it SHALL explain how to use a local checkout instead of the submodule

### Requirement: Clean Git History

The standalone filter-chain repository SHALL have a clean git history derived from the monorepo using `git filter-repo --subdirectory-filter filter-chain/`.

#### Scenario: History contains only filter-chain commits

- **GIVEN** the standalone filter-chain repository
- **WHEN** the git log is inspected
- **THEN** commits SHALL contain only changes relevant to the filter-chain subdirectory
- **AND** no commits that only modified goggles-specific files (outside `filter-chain/`) SHALL be present

#### Scenario: File paths are repository-root-relative

- **GIVEN** the standalone filter-chain repository after history rewrite
- **WHEN** file paths in the git history are inspected
- **THEN** paths SHALL be relative to the repository root (e.g., `src/`, `include/`, not `filter-chain/src/`)
- **AND** the `filter-chain/` prefix SHALL have been stripped from all paths
