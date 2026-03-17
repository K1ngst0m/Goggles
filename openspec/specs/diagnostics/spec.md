# Diagnostics Specification

## Purpose

Defines the structured diagnostic event model, sink-agnostic adapter contracts, layered activation tiers, reporting modes, session identity, and validation workflows that form the filter-chain diagnostics system. This domain covers the core infrastructure that all other diagnostic capabilities (authoring analysis, runtime validation, capture, quality validation) are built upon.

## Requirements

### Requirement: Diagnostic Event Model

The diagnostics system SHALL define a structured event type that carries severity, category, pass localization, and an evidence payload. Every diagnostic observation emitted by any layer of the system MUST use this event type.

#### Scenario: Event carries required fields
- GIVEN a diagnostic observation is generated during filter-chain operation
- WHEN the observation is emitted as a diagnostic event
- THEN the event SHALL include a severity level (error, warning, info, debug)
- AND the event SHALL include a category (authoring, runtime, quality, capture)
- AND the event SHALL include a localization key identifying the pass ordinal, processing stage, and affected resource or semantic name where applicable
- AND the event SHALL include a session identity reference

#### Scenario: Event carries optional evidence payload
- GIVEN a diagnostic event is emitted with supporting evidence
- WHEN a sink receives the event
- THEN the event MAY include a structured evidence payload containing resource identifiers, extent values, semantic values, image data references, or source location information
- AND the evidence payload format SHALL be defined by the event category

#### Scenario: Events without pass context use chain-level localization
- GIVEN a diagnostic observation applies to the chain as a whole rather than a specific pass
- WHEN the observation is emitted
- THEN the localization key SHALL use a sentinel pass ordinal indicating chain-level scope
- AND the processing stage SHALL identify the chain-level operation (preset parsing, graph validation, policy evaluation)

### Requirement: Severity Classification

The diagnostics system SHALL define a closed set of severity levels with deterministic ordering and policy-driven promotion rules.

#### Scenario: Severity levels are ordered
- GIVEN the set of diagnostic severity levels
- WHEN severity values are compared
- THEN the ordering SHALL be debug < info < warning < error
- AND no severity level outside this closed set SHALL be emittable

#### Scenario: Policy-driven severity promotion
- GIVEN a diagnostic policy configured in strict mode
- WHEN a fallback substitution event is emitted that would normally be a warning
- THEN the diagnostics system SHALL promote the event to error severity
- AND the original severity SHALL be preserved in the event metadata for audit

#### Scenario: Default severity in compatibility mode
- GIVEN a diagnostic policy configured in compatibility mode
- WHEN a fallback substitution event is emitted
- THEN the event SHALL retain warning severity
- AND the event SHALL be recorded in the degradation ledger

### Requirement: Sink-Agnostic Adapter Interface

The diagnostics system SHALL define a sink adapter interface that decouples diagnostic event production from event consumption. The interface SHALL consist of a single method that receives a diagnostic event.

#### Scenario: Multiple sinks receive the same event
- GIVEN two or more sink adapters are registered with the diagnostic session
- WHEN a diagnostic event is emitted
- THEN each registered sink adapter SHALL receive the event
- AND sink delivery order SHALL be deterministic within a session

#### Scenario: Sink failure does not block event emission
- GIVEN a sink adapter encounters an error during event processing
- WHEN the adapter returns from the event delivery call
- THEN the diagnostic session SHALL NOT halt event emission to other sinks
- AND the diagnostic session SHALL record the sink failure as a diagnostic event itself

#### Scenario: No sinks registered
- GIVEN a diagnostic session is created with no sink adapters registered
- WHEN diagnostic events are emitted
- THEN events SHALL be silently discarded
- AND no runtime error SHALL occur

### Requirement: Concrete Sink Adapters

The diagnostics system SHALL provide at minimum two concrete sink adapters: a logging sink and a test-harness sink.

#### Scenario: Logging sink routes to spdlog
- GIVEN a logging sink adapter is registered
- WHEN a diagnostic event is received
- THEN the adapter SHALL format the event and route it to spdlog using a dedicated logger name distinct from the main application logger
- AND the spdlog severity SHALL correspond to the diagnostic event severity

#### Scenario: Test-harness sink collects events for assertion
- GIVEN a test-harness sink adapter is registered in a test context
- WHEN diagnostic events are emitted during a test run
- THEN the adapter SHALL collect all events in an ordered, queryable container
- AND tests SHALL be able to assert on event count, severity, category, and localization fields

#### Scenario: Test-harness sink supports filtering
- GIVEN a test-harness sink with collected events
- WHEN a test queries events by category and severity
- THEN the sink SHALL return only events matching the specified filter criteria
- AND the returned events SHALL preserve emission order

### Requirement: Diagnostic Session Identity

Every diagnostic session SHALL carry an identity that uniquely identifies the validation context and is attached to every emitted event and every produced artifact.

#### Scenario: Session identity contains content hashes
- GIVEN a diagnostic session is created for a loaded preset
- WHEN the session identity is constructed
- THEN the identity SHALL include the preset revision hash, the expanded-source hash, and the compiled-contract hash

#### Scenario: Session identity contains runtime context
- GIVEN a diagnostic session is active during frame recording
- WHEN the session identity is inspected
- THEN the identity SHALL include the runtime generation identifier, the frame range, the capture mode, and an environment fingerprint

#### Scenario: Session identity is stable across repeated runs
- GIVEN the same preset with the same source content and the same environment
- WHEN two diagnostic sessions are created
- THEN the content hashes in both session identities SHALL be identical
- AND the runtime generation identifiers MAY differ

### Requirement: Layered Activation Tiers

The diagnostics system SHALL support three activation tiers that control the cost and depth of diagnostic instrumentation.

#### Scenario: Tier 0 baseline checks for an active session
- GIVEN a diagnostic session is active with the default diagnostics tier
- WHEN filter-chain frames are recorded
- THEN Tier 0 checks (binding coverage, semantic coverage, degradation detection) SHALL execute every frame for that session
- AND Tier 0 overhead SHALL be less than 1% of frame time

#### Scenario: No diagnostic session keeps runtime behavior unchanged
- GIVEN a filter-chain runtime has no active diagnostic session
- WHEN filter-chain frames are recorded
- THEN the runtime SHALL preserve existing rendering behavior without creating diagnostic artifacts
- AND diagnostics-only ledgers and sink delivery SHALL remain inactive until a session is created

#### Scenario: Tier 1 runtime opt-in
- GIVEN a build with Tier 1 diagnostics enabled via configuration
- WHEN filter-chain frames are recorded
- THEN additional diagnostic collection (GPU timestamp queries, selected readback, execution timeline) SHALL be active
- AND Tier 1 overhead SHALL be between 5% and 15% of frame time

#### Scenario: Tier 2 compile-time gated forensic capture
- GIVEN a build compiled without the forensic instrumentation compile flag
- WHEN the binary is inspected
- THEN no Tier 2 instrumentation symbols or code paths SHALL be present
- AND no Tier 2 overhead SHALL exist

#### Scenario: Tier 2 enabled at compile time
- GIVEN a build compiled with the forensic instrumentation compile flag enabled
- WHEN forensic capture is triggered
- THEN full intermediate readback, complete event timeline, and artifact bundle generation SHALL be available

#### Scenario: Tier 2 macros are compiled out by default
- GIVEN a build where `GOGGLES_DIAGNOSTICS_FORENSIC` is not defined
- WHEN forensic helper macros are included from `src/util/diagnostics/forensic.hpp`
- THEN `GOGGLES_DIAG_FORENSIC_SCOPE(name)` and `GOGGLES_DIAG_FORENSIC_CAPTURE(session, pass, cmd)` SHALL expand to no-op expressions
- AND no Tier 2 capture code SHALL be required by downstream translation units

### Requirement: Diagnostic Policy Configuration

The diagnostics system SHALL support a library-internal policy configuration that controls strict versus compatibility behavior, capture depth, retention limits, and per-degradation severity rules without exposing the policy struct through the public boundary.

#### Scenario: Strict mode forbids silent fallback
- GIVEN diagnostic policy is set to strict mode
- WHEN a texture binding resolves via fallback substitution
- THEN the diagnostics system SHALL emit an error-severity event
- AND the fallback SHALL be forbidden (the pass SHALL NOT execute with the substituted resource)

#### Scenario: Compatibility mode records fallback
- GIVEN diagnostic policy is set to compatibility mode
- WHEN a texture binding resolves via fallback substitution
- THEN the diagnostics system SHALL emit a warning-severity event
- AND the fallback SHALL proceed and the pass SHALL execute with the substituted resource
- AND the substitution SHALL be recorded in the degradation ledger

#### Scenario: Host boundary does not advertise diagnostics policy configuration
- GIVEN a Goggles host integrates through the current filter-chain boundary
- WHEN the application initializes
- THEN the host SHALL NOT expose a user-facing `[diagnostics]` policy surface that implies runtime policy control
- AND any diagnostics behavior beyond passive summary retrieval SHALL remain internal to the library

#### Scenario: Concrete policy shape remains internal
- GIVEN the runtime diagnostics policy is derived from host configuration and library defaults
- WHEN the active policy is applied inside the filter-chain implementation
- THEN the library MAY use whatever internal fields it requires to enforce diagnostics behavior
- AND the public boundary SHALL NOT expose the concrete policy struct shape as a contract

#### Scenario: Policy struct shape is not public API

- **GIVEN** the standalone filter-chain library's diagnostics policy type
- **WHEN** host code interacts through the public boundary
- **THEN** the library SHALL keep policy fields and policy object layout internal
- **AND** the public diagnostics contract SHALL remain limited to summary retrieval via `get_diagnostic_summary()`

#### Scenario: Host does not translate TOML diagnostics policy into runtime control

- **GIVEN** the Goggles application loads its TOML configuration
- **WHEN** it initializes filter-chain integration
- **THEN** no `[diagnostics]` TOML keys SHALL be required or interpreted as runtime policy controls
- **AND** the standalone library SHALL NOT import or link TOML parsing libraries

### Requirement: Reporting Modes

The diagnostics system SHALL support four reporting modes that control the breadth and depth of diagnostic output.

#### Scenario: Minimal mode output
- GIVEN reporting mode is set to Minimal
- WHEN a diagnostic report is generated
- THEN the report SHALL include the final verdict, compact failure summary, degraded-state markers, error counts by category, and session metadata
- AND the report SHALL NOT include intermediate image captures except final output on failure

#### Scenario: Standard mode output
- GIVEN reporting mode is set to Standard
- WHEN a diagnostic report is generated
- THEN the report SHALL include everything in Minimal plus normalized chain manifest, compile and reflection summaries, pass graph summary, binding coverage table, semantic coverage table, and one-frame execution trace

#### Scenario: Investigate mode output
- GIVEN reporting mode is set to Investigate
- WHEN a diagnostic report is generated
- THEN the report SHALL include everything in Standard plus selected intermediate outputs, uniform and push-constant dumps for selected passes, detailed source provenance, expanded degradation ledger, and history and feedback snapshots for selected frames

#### Scenario: Forensic mode output
- GIVEN reporting mode is set to Forensic
- WHEN a diagnostic report is generated
- THEN the report SHALL include everything in Investigate plus all pass outputs for selected frame ranges, full temporal capture of history and feedback resources, full event timeline, and complete artifact bundle with hashes and environment fingerprint

### Requirement: Degradation Ledger

The diagnostics system SHALL maintain a degradation ledger that records every instance of fallback substitution, unresolved semantic binding, missing reflection data, or policy downgrade during a diagnostic session.

#### Scenario: Fallback substitution is ledgered
- GIVEN a texture binding falls back to the source image during frame recording
- WHEN the binding is resolved
- THEN the degradation ledger SHALL record the pass ordinal, the expected resource identity, the substituted resource identity, and the frame index

#### Scenario: Unresolved semantic is ledgered
- GIVEN a semantic destination member has no matching semantic assignment
- WHEN semantic values are populated for a pass
- THEN the degradation ledger SHALL record the pass ordinal, the unresolved member name, and the destination offset

#### Scenario: Ledger is queryable by pass
- GIVEN a degradation ledger with multiple entries across different passes
- WHEN the ledger is queried for a specific pass ordinal
- THEN only degradation entries for that pass SHALL be returned
- AND entries SHALL be ordered by frame index

### Requirement: Chain Manifest

The diagnostics system SHALL produce a chain manifest that describes the normalized structure of a loaded preset including pass order, scaling configuration, texture declarations, alias declarations, and temporal requirements.

#### Scenario: Manifest reflects loaded preset
- GIVEN a preset has been successfully loaded and compiled
- WHEN the chain manifest is generated
- THEN the manifest SHALL list every pass with its ordinal, shader source path, scale type, scale factor, format override, and wrap mode
- AND the manifest SHALL list every declared alias with its target pass
- AND the manifest SHALL list every declared preset texture with its path

#### Scenario: Manifest includes temporal requirements
- GIVEN a preset uses history or feedback resources
- WHEN the chain manifest is generated
- THEN the manifest SHALL include the inferred history depth
- AND the manifest SHALL list every feedback-producing pass with its consumer passes

#### Scenario: Manifest is deterministic
- GIVEN the same preset is loaded twice in identical environments
- WHEN chain manifests are generated for both loads
- THEN both manifests SHALL be byte-identical

### Requirement: Binding Ledger

The diagnostics system SHALL produce a per-pass, per-frame binding ledger that records the resolved resource table including source identity, fallback status, resource extents, and producer relationship for every texture binding.

#### Scenario: All bindings are accounted for
- GIVEN an effect pass with reflected texture bindings
- WHEN the binding ledger is populated during frame recording
- THEN every reflected texture binding SHALL have an entry in the ledger
- AND each entry SHALL classify the binding as resolved, substituted (fallback), or unresolved

#### Scenario: Binding ledger records producer chain
- GIVEN a pass samples the output of an earlier pass via alias
- WHEN the binding ledger entry is created
- THEN the entry SHALL record the producing pass ordinal and the alias name used for resolution

#### Scenario: Binding ledger records extents
- GIVEN a texture binding is resolved to a concrete resource
- WHEN the binding ledger entry is created
- THEN the entry SHALL record the width, height, and format of the bound resource

### Requirement: Semantic Assignment Ledger

The diagnostics system SHALL produce a per-pass semantic assignment ledger that records resolved semantic values and their destination offsets in uniform buffers and push constants.

#### Scenario: All semantic destinations are classified
- GIVEN a pass with reflected uniform buffer and push constant members
- WHEN the semantic assignment ledger is populated
- THEN every destination member SHALL be classified as parameter, semantic, static, or unresolved

#### Scenario: Semantic values are recorded
- GIVEN a pass receives semantic injection for source size, output size, and frame count
- WHEN the semantic assignment ledger is populated
- THEN the ledger SHALL record the concrete values written for each semantic destination

#### Scenario: Alias-size destinations are resolved
- GIVEN a pass has destination members with size suffixes that resolve via the alias-size table
- WHEN the semantic assignment ledger is populated
- THEN the ledger SHALL record the resolved producer pass and the concrete size values

### Requirement: Execution Timeline

The diagnostics system SHALL produce an ordered execution timeline that records allocation, recording, history push, feedback copy, and final composition events with timestamps when Tier 1 or higher is active.

#### Scenario: Timeline records pass-level events
- GIVEN Tier 1 diagnostics are active
- WHEN effect passes are recorded for a frame
- THEN the execution timeline SHALL include a start and end event for each pass with timing data

#### Scenario: GPU timing is populated on recycled frame slots
- GIVEN Tier 1 diagnostics are active with GPU timestamps available
- WHEN a frame slot is reused after the previous submission has completed
- THEN pass end events for that slot SHALL be annotated with GPU duration in microseconds
- AND the first use of a slot MAY omit GPU duration until results become available

#### Scenario: Timeline records temporal operations
- GIVEN Tier 1 diagnostics are active and the preset uses history and feedback
- WHEN a frame completes recording
- THEN the execution timeline SHALL include history push events and feedback copy events with their respective timing data

#### Scenario: Timeline ordering matches execution order
- GIVEN an execution timeline for a single frame
- WHEN the timeline events are enumerated
- THEN events SHALL appear in the order they were executed
- AND pass events SHALL appear in pass-ordinal order

### Requirement: Authoring Verdict

The diagnostics system SHALL produce an authoring verdict after static validation that summarizes whether the preset is ready for execution, with categorized findings.

#### Scenario: Clean preset produces passing verdict
- GIVEN a preset where all passes compile, all reflections succeed, and all contracts validate
- WHEN the authoring verdict is generated
- THEN the verdict SHALL be "pass"
- AND the finding count for errors SHALL be zero

#### Scenario: Compilation failure produces failing verdict
- GIVEN a preset where one pass fails to compile
- WHEN the authoring verdict is generated
- THEN the verdict SHALL be "fail"
- AND the findings SHALL include at least one error-severity entry identifying the failing pass ordinal and compilation stage

#### Scenario: Reflection loss produces conditional verdict
- GIVEN a preset where compilation succeeds but reflection fails for one pass in compatibility mode
- WHEN the authoring verdict is generated
- THEN the verdict SHALL be "degraded"
- AND the findings SHALL include a warning identifying the pass ordinal and reflection stage
- AND the verdict SHALL note that strict mode would have produced a "fail" verdict

### Requirement: Source Provenance Map

The diagnostics system SHALL produce a source provenance map that links expanded and rewritten shader text back to original file paths and line numbers.

#### Scenario: Include expansion is tracked
- GIVEN a shader source that uses #include directives
- WHEN the source provenance map is generated after preprocessing
- THEN every line in the expanded source SHALL map back to a file path and line number in the original source tree

#### Scenario: Compatibility rewrites are tracked
- GIVEN a shader source that undergoes compatibility text rewrites during preprocessing
- WHEN the source provenance map is generated
- THEN rewritten regions SHALL map back to the original source location before rewriting
- AND the provenance entry SHALL indicate that a rewrite was applied

#### Scenario: Provenance enables error localization
- GIVEN a compilation error at a specific line in the expanded source
- WHEN the source provenance map is consulted
- THEN the original file path and line number SHALL be recoverable
- AND the diagnostic event for the compilation error SHALL include the original source location

### Requirement: Compile and Reflection Reports

The diagnostics system SHALL produce per-pass compile reports and reflection reports as structured artifacts.

#### Scenario: Compile report records per-stage results
- GIVEN shader compilation has been attempted for a pass
- WHEN the compile report is generated
- THEN the report SHALL include per-stage (vertex, fragment) compilation success or failure, diagnostic messages, timing, and cache-hit status

#### Scenario: Reflection report records merged contract
- GIVEN shader reflection has been performed for a pass
- WHEN the reflection report is generated
- THEN the report SHALL include per-stage reflected resources (uniform buffer members, push constant members, texture bindings, vertex inputs)
- AND the report SHALL include the merged pass contract with any binding collisions or type mismatches noted

#### Scenario: Reflection absence is explicitly reported
- GIVEN a pass where compilation succeeds but reflection produces an empty contract
- WHEN the reflection report is generated
- THEN the report SHALL explicitly flag reflection absence
- AND the severity SHALL be error in strict mode and warning in compatibility mode

### Requirement: Diagnostics Implementation Builds Without goggles_util

The diagnostics implementation (event model, sinks, session, ledger, policy, and related types)
SHALL build as part of the standalone filter-chain project without linking or depending on the
`goggles_util` CMake target. The diagnostics OBJECT library SHALL NOT transitively pull in
`toml11`, `BS_thread_pool`, or `Threads` as link dependencies.

#### Scenario: Standalone diagnostics target has no goggles_util dependency

- **GIVEN** the standalone project's diagnostics OBJECT library target
- **WHEN** its link dependencies are inspected (e.g., via `cmake --graphviz` or target property query)
- **THEN** `goggles_util` SHALL NOT appear as a direct or transitive link dependency
- **AND** `toml11`, `BS_thread_pool`, and `Threads` SHALL NOT appear as transitive link dependencies

#### Scenario: Diagnostics compiles outside Goggles source tree

- **GIVEN** the diagnostics implementation files have been moved to `filter-chain/src/diagnostics/`
- **WHEN** the standalone project is configured and built from a clean checkout
- **THEN** all diagnostics source files SHALL compile without errors
- **AND** compilation SHALL NOT require Goggles `src/util/` headers on the include path

#### Scenario: Diagnostics headers are self-contained within standalone tree

- **GIVEN** the ~10 diagnostics headers referenced by chain and shader code
- **WHEN** their transitive include dependencies are audited within the standalone tree
- **THEN** each header SHALL resolve all includes through standalone-owned paths
- **AND** no header SHALL include Goggles `util/config.hpp`, `util/job_system.hpp`, or other
  `goggles_util`-owned types

### Requirement: Host Boundary Does Not Expose Diagnostics Policy Configuration

The Goggles host boundary SHALL not expose TOML-based diagnostics policy configuration or other
user-facing controls that imply caller ownership of diagnostics policy. The standalone diagnostics
library SHALL NOT read TOML configuration files directly, and the public boundary SHALL NOT expose
policy-setting or diagnostic-session-creation API parameters.

#### Scenario: Standalone library keeps policy internal and avoids TOML

- **GIVEN** the standalone library evaluates diagnostics behavior during runtime operation
- **WHEN** policy decisions are applied
- **THEN** the library SHALL use internal policy state rather than public boundary API parameters
- **AND** the library SHALL NOT read TOML configuration files or depend on `toml11` for policy resolution

#### Scenario: Goggles host keeps diagnostics configuration out of runtime config

- **GIVEN** the Goggles application reads its TOML configuration
- **WHEN** it initializes filter-chain diagnostics behavior
- **THEN** the host SHALL limit caller-visible diagnostics behavior to passive summary retrieval
- **AND** the TOML parsing dependency SHALL remain in the Goggles host, not in the standalone library

### Requirement: Diagnostic Test Ownership Transfer

The standalone project SHALL own and run all diagnostics unit tests that verify diagnostic event
model, sink behavior, session lifecycle, ledger operations, and policy enforcement. Goggles SHALL
NOT retain copies of these tests.

#### Scenario: Diagnostics unit tests run from standalone project

- **GIVEN** diagnostics unit tests have been moved to `filter-chain/tests/`
- **WHEN** `ctest --test-dir filter-chain/build` is executed
- **THEN** all diagnostics unit tests SHALL pass using library-owned diagnostics test corpus assets
- **AND** test assertions SHALL verify event model, sink delivery, session identity, and ledger behavior

#### Scenario: Goggles does not retain diagnostics unit tests

- **GIVEN** the diagnostics unit test files have been moved to the standalone project
- **WHEN** the Goggles `tests/` directory is inspected
- **THEN** the moved diagnostics unit test files SHALL NOT be present
- **AND** Goggles MAY retain host integration tests that exercise public diagnostic summary retrieval
  through the filter-chain boundary API
