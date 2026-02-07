# profiling Specification

## Purpose
TBD - created by archiving change add-tracy-profiling. Update Purpose after archive.
## Requirements
### Requirement: Profiling Infrastructure

The system SHALL provide a compile-time toggleable profiling infrastructure via a CMake option `ENABLE_PROFILING`.

#### Scenario: Profiling disabled (default)

- **WHEN** `ENABLE_PROFILING` is `OFF` (default)
- **THEN** all profiling macros SHALL expand to no-ops
- **AND** no Tracy symbols or overhead SHALL be present in the binary

#### Scenario: Profiling enabled

- **WHEN** `ENABLE_PROFILING` is `ON`
- **THEN** profiling macros SHALL emit Tracy instrumentation
- **AND** the application SHALL be connectable to the Tracy profiler UI

### Requirement: Profiling Macro API

The system SHALL provide profiling macros in `src/util/profiling.hpp` that abstract the underlying profiler implementation.

#### Scenario: Scoped zone profiling

- **WHEN** `GOGGLES_PROFILE_SCOPE(name)` is used
- **THEN** a named profiling zone SHALL be created that ends when the scope exits

#### Scenario: Function profiling

- **WHEN** `GOGGLES_PROFILE_FUNCTION()` is used
- **THEN** a profiling zone named after the enclosing function SHALL be created

#### Scenario: Frame boundary marking

- **WHEN** `GOGGLES_PROFILE_FRAME(name)` is used
- **THEN** a frame boundary marker SHALL be emitted for frame-rate analysis

#### Scenario: Manual zone control

- **WHEN** `GOGGLES_PROFILE_BEGIN(name)` and `GOGGLES_PROFILE_END()` are used as a pair
- **THEN** a profiling zone SHALL span from BEGIN to END

#### Scenario: Zone annotation

- **WHEN** `GOGGLES_PROFILE_TAG(text)` is used within a zone
- **THEN** the zone SHALL be annotated with the provided text

#### Scenario: Numeric value plotting

- **WHEN** `GOGGLES_PROFILE_VALUE(name, value)` is used
- **THEN** the numeric value SHALL be recorded for time-series visualization

### Requirement: Render Pipeline Instrumentation

The system SHALL instrument performance-critical render pipeline functions with profiling zones.

#### Scenario: Frame render profiling

- **WHEN** `VulkanBackend::render_frame()` executes
- **THEN** profiling data SHALL capture the full frame render duration

#### Scenario: Filter chain profiling

- **WHEN** `FilterChain::record()` executes
- **THEN** profiling data SHALL capture the filter chain execution duration

#### Scenario: Per-pass profiling

- **WHEN** `FilterPass::record()` executes
- **THEN** profiling data SHALL capture individual pass durations with pass identification

### Requirement: Shader Compilation Instrumentation

The system SHALL instrument shader compilation functions with profiling zones.

#### Scenario: Shader compilation profiling

- **WHEN** `ShaderRuntime::compile_shader()` executes
- **THEN** profiling data SHALL capture compilation duration

#### Scenario: Cache operation profiling

- **WHEN** SPIR-V cache load or save operations execute
- **THEN** profiling data SHALL capture I/O duration

### Requirement: Capture Pipeline Instrumentation

The system SHALL instrument capture pipeline functions with minimal profiling to avoid performance impact.

#### Scenario: Frame capture profiling

- **WHEN** `CaptureManager::capture_frame()` executes
- **THEN** profiling data SHALL capture capture operation duration

#### Scenario: Capture layer hot path protection

- **WHEN** `vkQueuePresentKHR` hook executes
- **THEN** profiling overhead SHALL be minimal (entry marker only, no nested zones)

### Requirement: CMake Build Preset

The system SHALL provide a CMake preset for profiling builds.

#### Scenario: Profile preset usage

- **WHEN** building with `cmake --preset profile`
- **THEN** a Release build with profiling enabled SHALL be produced

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

