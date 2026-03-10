## MODIFIED Requirements

### Requirement: Repo-Controlled Semgrep Gate

The CI system SHALL run a repo-controlled Semgrep scan as a blocking step in the static-analysis
workflow.

The CI system SHALL allow the Semgrep blocking step to run in a dedicated PR-gated job that MAY run
in parallel with other static-analysis checks, provided equivalent required PR coverage is preserved.

#### Scenario: Static-analysis job runs checked-in Semgrep rules
- **GIVEN** the repository defines a Semgrep configuration and local ruleset
- **WHEN** the static-analysis workflow runs in CI
- **THEN** it SHALL execute Semgrep from repository-checked-in configuration
- **AND** it SHALL fail the job when Semgrep reports a blocking finding

#### Scenario: Local and CI Semgrep use repository-defined entrypoints
- **GIVEN** the repository exposes repository-owned Semgrep entrypoints for local and CI execution
- **WHEN** contributors run the local command and CI runs the static-analysis job graph
- **THEN** both flows SHALL evaluate the same checked-in ruleset
- **AND** both flows SHALL remain deterministic from repository state

#### Scenario: Semgrep complements the existing quality gate
- **GIVEN** the static-analysis workflow requires both Semgrep and `pixi run build -p quality`
- **WHEN** Semgrep is executed as part of the static-analysis PR checks
- **THEN** the workflow SHALL retain the existing quality build gate
- **AND** Semgrep SHALL complement rather than replace that gate

#### Scenario: Semgrep scope stays limited to approved policy bans
- **GIVEN** the repository policy identifies both tool-enforceable and review-only rules
- **WHEN** the CI Semgrep gate evaluates the repository
- **THEN** it SHALL cover only the approved high-signal policy bans selected for Semgrep
- **AND** it SHALL NOT duplicate formatting, naming, include-order, lockfile/preset, or
  runtime-validation checks that are owned by other tools

## ADDED Requirements

### Requirement: Static Analysis Coverage Supports Parallel PR Execution

The CI static-analysis coverage SHALL be partitioned into independently executable Semgrep and
quality-build surfaces that can run as separate required PR checks.

The repository SHALL preserve a combined static-analysis entrypoint for deterministic local
reproduction while exposing split entrypoints for targeted execution.

#### Scenario: PR checks run split static-analysis surfaces
- **GIVEN** pull-request CI executes static-analysis coverage
- **WHEN** the workflow schedules static-analysis jobs
- **THEN** it SHALL run Semgrep and quality-build static-analysis surfaces as separate jobs or lanes
- **AND** both surfaces SHALL remain required merge gates

#### Scenario: Combined local static-analysis entrypoint remains available
- **GIVEN** a contributor runs the canonical local static-analysis command
- **WHEN** the command executes
- **THEN** it SHALL run both Semgrep and quality-build static-analysis surfaces from repository-owned
  lane definitions
- **AND** it SHALL preserve deterministic behavior from repository state

#### Scenario: Split local entrypoints support targeted reproduction
- **GIVEN** a contributor needs to isolate one static-analysis surface
- **WHEN** the contributor invokes the split Semgrep or quality-build lane directly
- **THEN** each lane SHALL execute only its owned static-analysis surface
- **AND** each lane SHALL preserve the same policy and blocking semantics used by CI

#### Scenario: Static-analysis optimization keeps equivalent PR coverage
- **GIVEN** the static-analysis composition is changed for throughput
- **WHEN** CI protection rules evaluate required checks
- **THEN** PR coverage SHALL remain equivalent to running both Semgrep and quality-build checks
- **AND** required checks SHALL NOT be moved to non-PR triggers to satisfy this requirement
