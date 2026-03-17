# Delta for ci

## ADDED Requirements

### Requirement: Standalone Filter-Chain CI Pipeline

The standalone filter-chain repository SHALL have its own GitHub Actions CI pipeline that validates the library independently of the goggles monorepo CI. The pipeline SHALL be a required merge gate on the standalone repository's main branch.

#### Scenario: Format check lane

- **GIVEN** code is pushed to the standalone filter-chain repository
- **WHEN** the CI pipeline executes the format check lane
- **THEN** clang-format SHALL be applied to all `.c`, `.h`, `.cpp`, `.hpp` files
- **AND** the lane SHALL fail if formatting violations are detected (or auto-fix and push on non-fork branches)

#### Scenario: Build and test lane

- **GIVEN** the standalone CI pipeline runs
- **WHEN** the build-and-test lane executes
- **THEN** the project SHALL configure, build, and run all contract tests
- **AND** the lane SHALL fail if any test fails

#### Scenario: Consumer validation lane

- **GIVEN** the standalone CI pipeline runs
- **WHEN** the consumer validation lane executes
- **THEN** the project SHALL build, install, and run consumer validation for both STATIC and SHARED library outputs
- **AND** `find_package(GogglesFilterChain CONFIG REQUIRED)` SHALL succeed for both consumer validation projects
- **AND** the lane SHALL fail if any consumer validation project fails to compile, link, or execute

#### Scenario: Static analysis lane

- **GIVEN** the standalone CI pipeline runs
- **WHEN** the static analysis lane executes
- **THEN** clang-tidy (or equivalent quality build) SHALL analyze FC source files
- **AND** the lane SHALL fail if blocking findings are reported

#### Scenario: CI pipeline is the merge gate

- **GIVEN** a pull request is opened against the standalone repository's main branch
- **WHEN** CI pipeline results are evaluated for merge eligibility
- **THEN** all lanes (format, build+test, consumer validation, static analysis) SHALL be required checks
- **AND** the PR SHALL NOT be mergeable until all lanes pass

### Requirement: Goggles CI Submodule Awareness

The goggles monorepo CI pipeline SHALL correctly initialize and build with the filter-chain git submodule. CI SHALL NOT assume the filter-chain source exists as an in-repo directory.

#### Scenario: CI initializes submodule before build

- **GIVEN** the goggles CI pipeline runs on a clean checkout
- **WHEN** the build step begins
- **THEN** git submodules SHALL be initialized and updated before CMake configuration
- **AND** the filter-chain submodule SHALL be present at the expected path

#### Scenario: Goggles CI passes with submodule

- **GIVEN** the goggles CI pipeline with submodule integration
- **WHEN** `pixi run ci --runner container --cache-mode warm --lane all` executes
- **THEN** all existing CI lanes SHALL pass
- **AND** no lane SHALL fail due to missing filter-chain source files

### Requirement: Cross-Repository Verification Scope Split

CI ownership SHALL follow the narrowed public boundary. Standalone `filter-chain` CI SHALL own intermediate-pass golden and diagnostics-heavy verification that depends on internal capture-oriented capabilities. Goggles CI SHALL retain end-to-end host integration coverage against the durable public boundary.

#### Scenario: Standalone CI owns diagnostics-heavy FC verification

- **GIVEN** verification flows that assert on intermediate outputs or diagnostics-heavy golden baselines
- **WHEN** CI responsibility is reviewed after extraction
- **THEN** those flows SHALL run in standalone `filter-chain` CI
- **AND** they SHALL NOT remain required Goggles CI coverage for normal host integration validation

#### Scenario: Goggles CI validates only host-facing boundary behavior

- **GIVEN** the Goggles CI pipeline after extraction
- **WHEN** FC-related coverage is evaluated
- **THEN** Goggles CI SHALL validate host wiring and end-to-end public-boundary behavior
- **AND** it SHALL NOT depend on stable caller-facing pass capture or diagnostics-session lifecycle APIs to satisfy its coverage obligations
