## ADDED Requirements

### Requirement: Unified Profile Session Command

The system SHALL provide a profile-session command that orchestrates build, launch, capture, and
artifact generation for dual-process Goggles profiling.

#### Scenario: Start-pattern CLI for profile sessions

- **WHEN** a user runs `pixi run profile [goggles_args...] -- <app> [app_args...]`
- **THEN** the command SHALL parse arguments using the same split semantics as `pixi run start`
- **AND** it SHALL launch a profiling session without requiring manual Tracy command orchestration

### Requirement: Dual Raw Trace Capture

A profile session SHALL capture both instrumented process roles and persist separate raw traces.

#### Scenario: Capture viewer and layer traces in one run

- **WHEN** a profile session completes successfully
- **THEN** it SHALL produce a raw trace for the viewer/compositor process
- **AND** it SHALL produce a raw trace for the app-side layer process

#### Scenario: Client-role mapping is explicit

- **WHEN** raw traces are written
- **THEN** the session metadata SHALL record which client endpoint/process produced each trace

### Requirement: Merged Single-Timeline Artifact

A profile session SHALL emit one merged timeline artifact derived from both raw traces.

#### Scenario: Merge success path

- **WHEN** both raw traces are present and readable
- **THEN** the system SHALL generate one merged trace file containing both process timelines
- **AND** the merged trace SHALL be suitable for cross-process timeline inspection

#### Scenario: Alignment-marker-aware merge

- **WHEN** shared frame alignment markers are available in both raw traces
- **THEN** merge SHALL align timelines using those markers before writing the merged trace

#### Scenario: Alignment fallback

- **WHEN** shared alignment markers are missing or insufficient
- **THEN** merge SHALL fall back to relative-time alignment
- **AND** the session metadata SHALL include a warning describing reduced alignment confidence

### Requirement: Session Artifact Manifest

The system SHALL persist machine-readable metadata for every profile session.

#### Scenario: Manifest contains reproducibility metadata

- **WHEN** a session finishes
- **THEN** it SHALL write a manifest describing command line, timestamps, client mapping, and artifact paths
- **AND** it SHALL include warnings/errors encountered during capture or merge stages
