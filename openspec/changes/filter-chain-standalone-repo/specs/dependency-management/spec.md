## ADDED Requirements

### Requirement: Standalone Filter Runtime Dependency Pinning
When the host project consumes standalone `goggles-filter-chain` and `util-core` artifacts, dependency versions SHALL be explicitly pinned and lockfile-resolved to preserve reproducible builds.

#### Scenario: Locked standalone runtime dependency resolution
- **GIVEN** host dependency manifests include standalone runtime and util-core packages
- **WHEN** dependency resolution runs for host build workflows
- **THEN** standalone filter runtime and util-core versions SHALL be explicitly constrained
- **AND** exact resolved versions SHALL be captured in lock-managed state for reproducible installs

### Requirement: Lockfile Authority and Drift Enforcement
Standalone runtime consumption and publication workflows SHALL declare authoritative lockfiles, and CI SHALL fail when lock drift or unlocked resolution paths are detected.

#### Scenario: Lock drift validation
- **GIVEN** lockfiles and dependency manifests are present for host and standalone workflows
- **WHEN** CI dependency integrity checks run
- **THEN** lockfile and manifest versions SHALL remain consistent
- **AND** unlocked dependency resolution in release workflows SHALL be rejected

### Requirement: util-core Contract Separation
Shared utility functionality required by standalone filter runtime SHALL be provided through a dedicated `util-core` package contract separated from host-only utility dependencies.

#### Scenario: util-core boundary verification
- **GIVEN** utility dependency ownership and package contracts are available for audit
- **WHEN** dependency contracts are audited for standalone filter runtime consumption
- **THEN** required shared utility symbols SHALL be sourced through `util-core`
- **AND** host-only utility dependencies SHALL remain excluded from standalone runtime dependency closure

### Requirement: util-core Ownership and Version Policy
The `util-core` package SHALL define a single ownership authority and versioning policy that guarantees compatibility expectations for both standalone runtime and host consumers. Ownership authority and version-policy contract SHALL be published as release artifacts in machine-readable and human-readable form.

#### Scenario: util-core compatibility policy audit
- **GIVEN** util-core release metadata and consumer constraints are available
- **WHEN** compatibility policy checks run
- **THEN** util-core ownership and release authority SHALL be explicit
- **AND** runtime and host compatibility expectations SHALL be versioned, testable, and represented in published policy artifacts

### Requirement: Standalone Workflow Policy Compliance
Standalone build/test/release workflows SHALL execute through Pixi task entrypoints and CMake/CTest presets, and SHALL NOT introduce ad-hoc non-preset build flows.

#### Scenario: Workflow policy conformance check
- **GIVEN** standalone CI workflow definitions are available
- **WHEN** workflow conformance checks run
- **THEN** required build/test/release stages SHALL invoke Pixi task entrypoints and preset-based CMake/CTest commands
- **AND** ad-hoc non-preset build invocations SHALL be rejected

### Requirement: Artifact Integrity and Channel Allowlisting
Standalone publication and host intake workflows SHALL require cryptographic integrity and publisher-authenticity metadata, and SHALL allow publication/consumption only through approved channels. Approved channels SHALL be declared in a version-controlled allowlist policy used by CI gate checks.

#### Scenario: Integrity and provenance enforcement
- **GIVEN** a standalone release candidate and channel policy definitions are available
- **WHEN** publication or host intake verification runs
- **THEN** artifact checksums and signature/attestation metadata SHALL validate successfully
- **AND** publication/consumption outside approved channels SHALL be rejected
