## ADDED Requirements

### Requirement: Standalone Repository Ownership Boundary
The `goggles-filter-chain` runtime SHALL be maintained as a standalone repository whose owned implementation boundary includes filter chain orchestration, shader runtime processing, and preset texture loading internals.

#### Scenario: Runtime ownership scope audit
- **GIVEN** the standalone runtime repository and host repository are both available for ownership review
- **WHEN** repository ownership and source boundaries are audited
- **THEN** runtime modules equivalent to chain/shader/texture internals SHALL be owned by the standalone repository
- **AND** host backend/app/ui/compositor modules SHALL remain outside standalone repository ownership

### Requirement: Versioned util-core Dependency Contract
The standalone `goggles-filter-chain` runtime SHALL consume shared utility functionality only through a versioned `util-core` dependency contract and SHALL NOT depend on host-application-only utility modules.

#### Scenario: Utility dependency boundary check
- **GIVEN** standalone runtime dependency manifests and link/include graphs are available
- **WHEN** standalone runtime link and include dependencies are validated
- **THEN** required shared utilities SHALL resolve through `util-core` contract packages
- **AND** dependencies that are host-app-only or UI/compositor-only SHALL NOT be required by the standalone runtime

### Requirement: Standalone Publication Readiness Gate
The standalone `goggles-filter-chain` repository SHALL publish versioned release artifacts only after required standalone build, scoped test, and compatibility validation gates pass. Required validation gates SHALL execute through Pixi task entrypoints and CMake/CTest presets. Required compatibility evidence SHALL include an ABI-diff report against the v1 baseline, an exported-symbol manifest diff, and a packaged-header contract check output.

#### Scenario: Release gate enforcement
- **GIVEN** a standalone release candidate and its CI validation outputs are available
- **WHEN** a standalone release candidate is prepared
- **THEN** required build and scoped test gates SHALL pass before publication
- **AND** release artifacts SHALL be versioned and accompanied by required compatibility evidence artifacts

### Requirement: Milestone-1 Release Profile and Artifact Manifest
Milestone-1 standalone releases SHALL target Linux `x86_64` and SHALL validate static/shared runtime outputs across clang and gcc release lanes. Published artifacts SHALL include a source bundle, public headers, static/shared runtime libraries, package metadata for consumers, and an artifact manifest.

#### Scenario: Release profile and manifest validation
- **GIVEN** a milestone-1 release candidate is prepared for publication
- **WHEN** release profile checks run
- **THEN** Linux `x86_64` build/test lanes for clang and gcc static/shared outputs SHALL succeed
- **AND** the published artifact set SHALL include the required manifest-defined outputs
