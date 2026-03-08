# build-system Spec Delta

## ADDED Requirements

### Requirement: Deterministic Semgrep Tooling

The build system SHALL provide a Pixi-managed Semgrep toolchain and checked-in rule source so local and CI static analysis use the same deterministic inputs.

#### Scenario: Pixi provides Semgrep for local and CI execution
- **GIVEN** the repository defines lint and developer workflow tooling in `pixi.toml`
- **WHEN** contributors or CI invoke the Semgrep entrypoint
- **THEN** the Semgrep binary SHALL come from the repository-managed Pixi environment
- **AND** the same Semgrep version surface SHALL be used locally and in CI

#### Scenario: Semgrep provenance is observable during verification
- **GIVEN** the repository verifies the Semgrep tool surface before enforcing the gate
- **WHEN** maintainers inspect the Semgrep path and version under the repository-managed workflow
- **THEN** the resolved Semgrep executable SHALL originate from the Pixi-managed environment
- **AND** the reported version SHALL match the locked local and CI Semgrep surface

#### Scenario: Pixi source-of-truth files stay synchronized
- **GIVEN** the repository adds Semgrep to the Pixi-managed tool surface
- **WHEN** the change updates Semgrep dependency configuration
- **THEN** `pixi.toml` SHALL declare the dependency version surface
- **AND** `pixi.lock` SHALL be updated in sync with that change

#### Scenario: Dependency admission remains reviewable
- **GIVEN** the repository adds Semgrep as a new dependency for the static-analysis workflow
- **WHEN** the proposal and apply artifacts are reviewed
- **THEN** they SHALL include dependency rationale, license compatibility review, maintenance assessment, and team agreement evidence
- **AND** the dependency SHALL NOT be treated as implicitly admitted just because the tool is easy to install

#### Scenario: Initial scan roots stay limited to repository-managed C and C++ code
- **GIVEN** the repository enables Semgrep policy checks
- **WHEN** the `pixi run semgrep` entrypoint runs in its initial configuration
- **THEN** it SHALL scan repository-managed code under `src/` and `tests/`
- **AND** it SHALL use narrower path filters for rules that apply only to selected subsystems

#### Scenario: Semgrep rule sources are checked into the repository
- **GIVEN** the repository enables Semgrep policy checks
- **WHEN** the Semgrep entrypoint runs
- **THEN** it SHALL load configuration and rules from checked-in repository files
- **AND** it SHALL NOT depend on registry defaults or hosted rule configuration

#### Scenario: Subsystem-sensitive rules stay path-scoped
- **GIVEN** some policy bans only apply to selected Goggles subsystems
- **WHEN** the repository defines Semgrep rules for Vulkan API split or render-path threading
- **THEN** those rules SHALL scope to the directories where the policy applies
- **AND** they SHALL exclude directories with explicit policy exceptions such as `src/capture/vk_layer/`
