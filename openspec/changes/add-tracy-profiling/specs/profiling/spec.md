## ADDED Requirements

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
