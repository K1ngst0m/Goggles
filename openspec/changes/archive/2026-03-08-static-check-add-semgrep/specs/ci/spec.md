# ci Spec Delta

## Purpose
Define the CI behavior change that adds Semgrep as a repo-controlled blocking gate in the existing static-analysis workflow.

## ADDED Requirements

### Requirement: Repo-Controlled Semgrep Gate

The CI system SHALL run a repo-controlled Semgrep scan as a blocking step in the static-analysis workflow.

#### Scenario: Static-analysis job runs checked-in Semgrep rules
- **GIVEN** the repository defines a Semgrep configuration and local ruleset
- **WHEN** the static-analysis workflow runs in CI
- **THEN** it SHALL execute Semgrep from repository-checked-in configuration
- **AND** it SHALL fail the job when Semgrep reports a blocking finding

#### Scenario: Local and CI Semgrep use the same repository entrypoint
- **GIVEN** the repository exposes `pixi run semgrep`
- **WHEN** contributors run the local command and CI runs the static-analysis job
- **THEN** both flows SHALL use the same repository-defined entrypoint
- **AND** both flows SHALL evaluate the same checked-in ruleset

#### Scenario: Semgrep complements the existing quality gate
- **GIVEN** the static-analysis workflow already runs `pixi run build -p quality`
- **WHEN** Semgrep is added to the workflow
- **THEN** the workflow SHALL retain the existing quality build gate
- **AND** Semgrep SHALL complement rather than replace that gate

#### Scenario: Semgrep scope stays limited to approved policy bans
- **GIVEN** the repository policy identifies both tool-enforceable and review-only rules
- **WHEN** the CI Semgrep gate evaluates the repository
- **THEN** it SHALL cover only the approved high-signal policy bans selected for Semgrep
- **AND** it SHALL NOT duplicate formatting, naming, include-order, lockfile/preset, or runtime-validation checks that are owned by other tools
