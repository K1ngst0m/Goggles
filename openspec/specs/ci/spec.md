# ci Specification

## Purpose
Defines the Continuous Integration pipeline behavior for the Goggles project, including automated code formatting, building, testing, and static analysis.
## Requirements
### Requirement: Auto-format Code on Push

The CI system SHALL automatically format code using the Pixi-managed clang-format and gate subsequent jobs on whether formatting changes were pushed.

#### Scenario: Code with formatting issues is pushed
- **WHEN** code with clang-format violations is pushed to a branch
- **THEN** CI runs clang-format to fix the issues via Pixi
- **AND** CI commits the formatted code with message \"style: auto-format code\" when the branch is non-fork
- **AND** the format check job succeeds

#### Scenario: Forked PR formatting without push
- **GIVEN** a pull request originates from a fork
- **WHEN** clang-format produces changes
- **THEN** CI SHALL skip auto-commit/push for safety
- **AND** it SHALL expose `formatted=false` so downstream jobs still run

#### Scenario: Code is already properly formatted
- **WHEN** code that passes clang-format check is pushed
- **THEN** CI detects no changes needed
- **AND** no commit is created
- **AND** the format check job succeeds

#### Scenario: All C/C++ file types are formatted
- **WHEN** clang-format is run in CI
- **THEN** files with extensions `.c`, `.h`, `.cpp`, `.hpp` are formatted
- **AND** the same clang-format version from Pixi is used

### Requirement: Unified Clang-Format via Pixi

The project SHALL use Pixi-managed formatting tools to ensure consistent formatting across all contributors regardless of their local system toolchain.

#### Scenario: Formatting uses Pixi-managed tools
- **WHEN** a contributor runs `pixi run format`
- **THEN** C/C++ formatting SHALL use `clang-format` from the Pixi environment
- **AND** TOML formatting SHALL use `taplo` from the Pixi environment

#### Scenario: Init installs the managed formatting hook
- **WHEN** a contributor runs `pixi run init`
- **THEN** the managed pre-commit hook SHALL be installed or repaired
- **AND** the hook SHALL format staged C/C++ and TOML files via Pixi-managed tools

### Requirement: Scoped Sanitizer Instrumentation

The build system SHALL apply sanitizer instrumentation only to first-party Goggles code, excluding all third-party dependencies.

#### Scenario: First-party target with ASAN enabled
- **WHEN** building with `ENABLE_ASAN=ON`
- **AND** the target is a first-party Goggles library or executable
- **THEN** the target is compiled with `-fsanitize=address -fno-omit-frame-pointer`
- **AND** the target is linked with `-fsanitize=address`

#### Scenario: Third-party dependency with ASAN enabled
- **WHEN** building with `ENABLE_ASAN=ON`
- **AND** the target is a third-party dependency (CPM or Conan)
- **THEN** the target is NOT compiled with sanitizer flags
- **AND** the target is NOT linked with sanitizer flags

### Requirement: Runtime Leak Suppressions

The build system SHALL configure LSAN suppressions via external file rather than compiled-in code, enabling transparent suppression of third-party library leaks while detecting first-party leaks.

#### Scenario: CTest runs with LSAN suppressions
- **WHEN** running tests via CTest with `ENABLE_ASAN=ON`
- **THEN** `LSAN_OPTIONS` environment variable includes `suppressions=<path>/lsan.supp`
- **AND** leaks originating entirely within suppressed patterns are not reported
- **AND** leaks in first-party code ARE reported even if called via third-party APIs

### Requirement: Scoped Clang-Tidy Configuration

The build system SHALL apply clang-tidy static analysis with per-directory configuration, allowing different strictness levels for different code categories.

#### Scenario: Application code with clang-tidy enabled
- **WHEN** building with `ENABLE_CLANG_TIDY=ON`
- **AND** the source file is outside directories with a local `.clang-tidy` override
- **THEN** the file is analyzed with the root `.clang-tidy` configuration
- **AND** all checks are enforced with `-warnings-as-errors`

#### Scenario: Generated protocol headers with clang-tidy enabled
- **WHEN** building with `ENABLE_CLANG_TIDY=ON`
- **AND** the source file is in `src/compositor/protocol/`
- **THEN** the file is analyzed with `src/compositor/protocol/.clang-tidy`
- **AND** all clang-tidy checks are disabled for those generated headers

#### Scenario: C filter-chain API code with clang-tidy enabled
- **WHEN** building with `ENABLE_CLANG_TIDY=ON`
- **AND** the source file is in `src/render/chain/api/c/`
- **THEN** the file inherits the root `.clang-tidy` configuration
- **AND** identifier naming and selected modernization checks are relaxed for the C-facing API surface

#### Scenario: Root clang-tidy enforces project conventions
- **WHEN** the root `.clang-tidy` is used
- **THEN** private members MUST use `m_` prefix
- **AND** enum constants MUST use `UPPER_SNAKE_CASE`
- **AND** functions MUST use `snake_case`
- **AND** C-style arrays are forbidden in favor of `std::array`

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

