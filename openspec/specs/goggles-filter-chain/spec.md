# goggles-filter-chain Specification

## Purpose
Define the boundary contract between the host render/backend code and the standalone
`goggles-filter-chain` runtime, including ownership, lifecycle, controls, and diagnostics.
## Requirements
### Requirement: Standalone Filter Library Target

The extracted filter runtime SHALL be an independently buildable, installable, and exportable
standalone CMake project that publishes the downstream target contract `goggles-filter-chain`.
The standalone project SHALL use repository layout rooted at `include/`, `src/`, `tests/`,
`assets/`, and `cmake/`. Release acceptance SHALL require `STATIC` and `SHARED` library outputs,
and SHALL NOT require or expose a `MODULE` library variant as part of the package contract.

#### Scenario: Target dependency direction
- **GIVEN** build targets are configured for render and filter runtime
- **WHEN** dependency checks run for target link relationships
- **THEN** `goggles-filter-chain` SHALL compile and link without depending on host backend targets
- **AND** host backend targets SHALL depend on `goggles-filter-chain` for filter execution

#### Scenario: Target dependency audit
- **GIVEN** the `goggles-filter-chain` target link dependency list
- **WHEN** dependency audit checks execute
- **THEN** `goggles-filter-chain` SHALL NOT link app- or UI-only targets
- **AND** `goggles-filter-chain` SHALL link only dependencies required for chain/shader/texture runtime behavior

#### Scenario: Standalone checkout builds without Goggles repository
- **GIVEN** a clean checkout of the extracted filter-chain project
- **WHEN** the documented CMake workflow configures and builds the project
- **THEN** the project SHALL build without requiring the Goggles source tree, Pixi wrappers, or Conda-specific paths
- **AND** the project layout consumed by that workflow SHALL be rooted at `include/`, `src/`, `tests/`, `assets/`, and `cmake/`

#### Scenario: Installed package preserves stable target identity
- **GIVEN** the standalone project has been installed and exported
- **WHEN** a downstream consumer resolves the package through CMake package discovery
- **THEN** the consumer SHALL obtain the filter runtime through the target contract `goggles-filter-chain`
- **AND** consuming the installed package SHALL NOT require downstream target renaming or source-tree include assumptions

#### Scenario: Distribution excludes module-only success criteria
- **GIVEN** the standalone project is prepared for release validation
- **WHEN** library artifacts and exported targets are inspected
- **THEN** `STATIC` and `SHARED` outputs SHALL both be available as supported deliverables
- **AND** no `MODULE` library variant SHALL be required for success or documented as part of the supported package surface

### Requirement: Complete Filter Runtime Ownership Boundary
The filter boundary SHALL own filter-chain orchestration, shader runtime ownership/creation,
shader processing, and preset texture loading internals. When the host retargets output format
without changing the active preset, the boundary SHALL preserve source-independent
preset-derived runtime state across that retarget.

#### Scenario: Source-independent preset work survives output retarget
- **GIVEN** a preset runtime has already completed parsing, shader compilation/reflection, preset
  texture loading, and effect-pass setup
- **WHEN** the host requests output-format retargeting for a source color-space change
- **THEN** that source-independent preset-derived work SHALL remain available after the retarget
- **AND** the boundary SHALL expose the same effect-stage behavior after activation

### Requirement: Host Backend Responsibility Boundary
The host backend SHALL remain responsible for swapchain lifecycle, external image import,
synchronization, queue submission, and present. The host backend SHALL use boundary-facing
retarget behavior for swapchain/output-format changes and SHALL reserve full preset/runtime
rebuild behavior for explicit preset reload requests.

#### Scenario: Format retarget is handed off without full preset rebuild
- **GIVEN** swapchain output format must change because the source color-space classification changed
- **WHEN** host backend recreation is triggered
- **THEN** host backend code SHALL recreate swapchain and present-path resources
- **AND** filter runtime retargeting SHALL be invoked through boundary-facing contracts without
  forcing full preset rebuild behavior

#### Scenario: Explicit preset reload still uses rebuild path
- **GIVEN** the user explicitly requests a preset reload or selects a different preset
- **WHEN** the host backend coordinates the change
- **THEN** the boundary interaction SHALL use the full preset/runtime rebuild path
- **AND** the request SHALL NOT be collapsed into output-format-only retarget behavior

#### Scenario: Pending runtime is aligned before activation
- **GIVEN** the host backend has a pending runtime from an explicit reload
- **AND** the authoritative output target changes before that pending runtime becomes active
- **WHEN** the backend/controller prepares the pending runtime for activation
- **THEN** the boundary interaction SHALL align that pending runtime to the current output target
- **AND** activation SHALL occur only after the pending runtime matches the latest output format

### Requirement: Boundary-safe VulkanContext Contract Placement
Host<->filter initialization contracts SHALL use a boundary-owned `VulkanContext` definition that does not pull backend internals into the filter boundary.

#### Scenario: VulkanContext ownership and include safety
- **GIVEN** headers used to define `VulkanContext` for host<->filter initialization
- **WHEN** include/dependency checks run
- **THEN** the `VulkanContext` type SHALL be declared in a boundary-owned header
- **AND** that header SHALL include only boundary-allowed dependencies and SHALL NOT include backend-only helper headers

#### Scenario: Host/backend consumption of VulkanContext contract
- **GIVEN** backend and filter runtime initialization paths
- **WHEN** host code passes initialization context into `goggles-filter-chain`
- **THEN** host code SHALL consume the boundary-owned `VulkanContext` contract
- **AND** backend public headers SHALL NOT expose filter-boundary internals beyond this contract

### Requirement: Boundary-safe Vulkan Result Utility Contracts
Filter boundary code SHALL use boundary-safe Vulkan result utilities and SHALL NOT include backend-only helper headers.

#### Scenario: Backend helper include removal
- **GIVEN** chain/shader/texture boundary source files
- **WHEN** include dependency checks are executed
- **THEN** backend helper headers SHALL NOT be included from boundary sources

#### Scenario: Boundary-safe `VK_TRY` relocation
- **GIVEN** Vulkan result-checking macros used by boundary code
- **WHEN** boundary-safe utility contracts are applied
- **THEN** boundary call sites SHALL include `VK_TRY` from a boundary-safe helper header
- **AND** that helper header SHALL depend only on boundary-allowed headers

### Requirement: Boundary-safe Control Descriptor Contract
The filter boundary SHALL expose curated control descriptors for both effect and prechain controls with a closed, deterministic stage contract.

#### Scenario: Control descriptor enumeration
- **GIVEN** a preset with effect-stage and prechain controls is loaded
- **WHEN** controls are enumerated through the boundary API
- **THEN** each descriptor SHALL include `control_id`, `stage`, `name`, `current_value`, `default_value`, `min_value`, `max_value`, and `step`
- **AND** descriptors SHALL represent both effect and prechain controls through the same boundary-safe contract

#### Scenario: Stage domain is explicit and closed
- **GIVEN** control descriptors returned by the boundary API
- **WHEN** descriptor stage values are validated
- **THEN** each descriptor `stage` SHALL be one of `prechain` or `effect`
- **AND** unknown stage values SHALL NOT be emitted without an explicit spec update

#### Scenario: Deterministic descriptor ordering
- **GIVEN** the same preset is enumerated repeatedly without control-layout changes
- **WHEN** control descriptors are listed through the boundary API
- **THEN** descriptor order SHALL be deterministic across runs and equivalent reloads
- **AND** ordering SHALL group `prechain` controls before `effect` controls while preserving stable per-stage ordering

#### Scenario: Optional description fallback
- **GIVEN** a control descriptor may omit `description`
- **WHEN** UI renders control metadata
- **THEN** UI SHALL render a control label from `name`
- **AND** UI SHALL apply no tooltip text when `description` is absent

### Requirement: Control Identifier Semantics
The control identifier contract SHALL define uniqueness and stability rules for `control_id`.

#### Scenario: Uniqueness within active preset
- **GIVEN** controls for a loaded preset are enumerated
- **WHEN** the boundary returns control descriptors
- **THEN** each `control_id` SHALL be unique within the active preset

#### Scenario: Stability across equivalent reload
- **GIVEN** the same preset is reloaded without control-layout changes
- **WHEN** controls are enumerated after reload
- **THEN** `control_id` values for matching controls SHALL remain stable across reload

#### Scenario: Different preset layouts
- **GIVEN** a different preset with different control layout is loaded
- **WHEN** controls are enumerated for the new preset
- **THEN** `control_id` values MAY differ from the previous preset

### Requirement: Control Mutation Contract
Control mutation and callback contracts SHALL use `control_id` and SHALL define deterministic out-of-range handling.

#### Scenario: Control mutation is `control_id`-only
- **GIVEN** boundary consumers set or reset control values
- **WHEN** control mutation APIs are invoked
- **THEN** operations SHALL address controls by `control_id`
- **AND** boundary surfaces SHALL NOT expose pass indices as mutation keys

#### Scenario: Out-of-range value handling
- **GIVEN** a control descriptor defines `min_value` and `max_value`
- **WHEN** a set-value request provides a value outside `[min_value, max_value]`
- **THEN** the boundary SHALL clamp the request to the nearest valid bound before applying it
- **AND** subsequent control enumeration SHALL report the clamped `current_value`

### Requirement: Adapter Ownership Isolation
Adapters from shader-internal metadata to curated control descriptors SHALL live behind the `goggles-filter-chain` boundary.

#### Scenario: Adapter dependency boundary
- **GIVEN** backend, app, and UI modules consume boundary descriptors
- **WHEN** control metadata adaptation is performed
- **THEN** adaptation from shader-internal metadata SHALL occur inside `goggles-filter-chain`
- **AND** backend/app/UI modules SHALL NOT include shader-internal metadata types for control enumeration

#### Scenario: Adapter mapping parity across effect and prechain sources
- **GIVEN** control metadata comes from effect-stage and prechain-stage sources with different field availability
- **WHEN** adapters build curated descriptors
- **THEN** both sources SHALL map to the same descriptor schema with documented, deterministic field mapping rules
- **AND** when a source omits `current_value`, adapters SHALL emit `current_value` equal to the effective runtime value (or `default_value` when no runtime override exists)

#### Scenario: UI/include isolation from shader internals
- **GIVEN** non-boundary consumer paths such as `src/ui` and `src/app`
- **WHEN** boundary compliance is validated through tests and source audit
- **THEN** non-boundary consumers SHALL NOT include `render/shader/*` headers for control metadata access

### Requirement: No Concrete FilterChain Type Exposure Outside Boundary
Backend public APIs, app code, and UI code MUST NOT depend on concrete chain headers, concrete `FilterChain*` types, or chain accessors that expose concrete internals.

#### Scenario: Include guard in app and UI
- **GIVEN** source files under `src/app` and `src/ui`
- **WHEN** boundary compliance is validated through tests and source audit
- **THEN** files under `src/app` and `src/ui` SHALL NOT include `render/chain/filter_chain.hpp`

#### Scenario: Backend public header guard
- **GIVEN** backend public headers consumed by app/UI and downstream tests
- **WHEN** boundary compliance is validated through tests and source audit
- **THEN** backend public headers SHALL NOT expose concrete `FilterChain` types or accessors returning concrete chain internals

#### Scenario: Type and accessor guard in app and UI
- **GIVEN** source files under `src/app` and `src/ui`
- **WHEN** boundary compliance is validated through tests and source audit
- **THEN** no direct references to concrete `FilterChain*` SHALL exist in app or UI code
- **AND** app/UI code SHALL NOT call backend chain-accessor methods that expose concrete chain internals

### Requirement: Stable Facade API Groups for Downstream Tests
The stable downstream test surface SHALL be limited to boundary facade groups for lifecycle/preset, frame submission, controls, and prechain/policy operations.

#### Scenario: Downstream test compile surface
- **GIVEN** downstream contract tests compile against the boundary facade
- **WHEN** tests include boundary-facing headers only
- **THEN** tests SHALL be able to exercise lifecycle/preset, frame submission, controls, and prechain/policy operations
- **AND** tests SHALL NOT require concrete chain, pass, shader-runtime, or deferred-destroy internal types

### Requirement: Facade Active-Chain Invocation Safety
Boundary facade methods SHALL resolve active chain/runtime references at call time and SHALL NOT cache concrete chain pointers across calls.

#### Scenario: Async swap with subsequent facade calls
- **GIVEN** an async preset reload swaps in a new active chain/runtime
- **WHEN** subsequent facade operations are invoked
- **THEN** each facade call SHALL target the current active chain/runtime reference
- **AND** facade operations SHALL NOT use stale cached concrete chain pointers from prior calls

### Requirement: Error Model and Diagnostics Contract

All fallible APIs MUST return `goggles_chain_status_t` and MUST NOT surface exceptions as part of the public contract. `goggles_chain_status_to_string(...)` MUST return a stable static string and unknown status values MUST map to `"UNKNOWN_STATUS"`. Structured diagnostics MUST remain limited to `goggles_chain_error_last_info_get(...)` and `goggles_fc_chain_get_diagnostic_summary(...)`; the boundary MUST NOT expose diagnostic event, artifact, or session-query retrieval APIs.

#### Scenario: Summary supplements last-error
- **GIVEN** a READY runtime records diagnostics internally and an API call fails
- **WHEN** the host queries diagnostics through the public boundary
- **THEN** `goggles_chain_error_last_info_get(...)` SHALL still return the last error info
- **AND** `goggles_fc_chain_get_diagnostic_summary(...)` SHALL remain the only public diagnostics summary surface beyond last-error details

#### Scenario: Diagnostics-not-active status has a stable code
- **GIVEN** a READY runtime where internal diagnostics are not active
- **WHEN** the host calls `goggles_fc_chain_get_diagnostic_summary(...)`
- **THEN** the boundary SHALL return `GOGGLES_CHAIN_STATUS_DIAGNOSTICS_NOT_ACTIVE`
- **AND** `goggles_chain_status_to_string(...)` SHALL map that code to `"DIAGNOSTICS_NOT_ACTIVE"`

### Requirement: Internal Diagnostic Session Lifecycle

The filter-chain runtime MAY manage diagnostic sessions internally, but the public boundary SHALL NOT expose session creation, query, or teardown controls.

#### Scenario: Host does not create diagnostic session with policy
- **GIVEN** a READY filter-chain runtime instance
- **WHEN** the host uses the public filter-chain boundary
- **THEN** no boundary API SHALL accept diagnostic session creation inputs or diagnostics policy parameters
- **AND** any diagnostics policy decisions SHALL remain library-internal

#### Scenario: Host queries diagnostic summary only
- **GIVEN** internal diagnostics state exists on a filter-chain runtime
- **WHEN** the host queries diagnostics through the boundary API
- **THEN** the boundary SHALL expose only `goggles_fc_chain_get_diagnostic_summary(...)`
- **AND** the public contract SHALL NOT include policy mode or session-state query fields

#### Scenario: No public session state is required
- **GIVEN** a READY filter-chain runtime with no host-visible diagnostics controls
- **WHEN** the runtime records frames
- **THEN** the runtime SHALL continue operating without any public session lifecycle interaction
- **AND** only the public summary API MAY report diagnostics availability

### Requirement: No Public Diagnostic Sink Registration

The filter-chain boundary SHALL NOT allow host code to register or unregister diagnostic sinks or callbacks.

#### Scenario: Host cannot register a sink adapter
- **GIVEN** a host integrates through the public filter-chain boundary
- **WHEN** it inspects available diagnostics APIs
- **THEN** no sink registration entrypoint SHALL be present
- **AND** diagnostic delivery SHALL NOT depend on host callback registration

#### Scenario: Host cannot unregister a sink adapter
- **GIVEN** a host integrates through the public filter-chain boundary
- **WHEN** it looks for a sink-unregistration API
- **THEN** no such public API SHALL exist
- **AND** the boundary SHALL expose no sink identifier contract

#### Scenario: Diagnostics do not promise external callback delivery
- **GIVEN** diagnostics are emitted internally by the runtime
- **WHEN** external consumers use the public boundary
- **THEN** the public contract SHALL make no guarantees about callback delivery ordering or multi-sink fanout
- **AND** host-visible diagnostics SHALL remain limited to summary retrieval

### Requirement: Public Diagnostics Retrieval Is Summary-Only

The filter-chain boundary SHALL expose diagnostic retrieval only through `goggles_fc_chain_get_diagnostic_summary(...)` without requiring external consumers to access runtime internals.

#### Scenario: Host retrieves diagnostic summary counts
- **GIVEN** internal diagnostics are active and events have been emitted
- **WHEN** the host calls `goggles_fc_chain_get_diagnostic_summary(...)`
- **THEN** the boundary SHALL return the public diagnostic summary data without exposing policy mode or event stream details
- **AND** the call SHALL return `GOGGLES_CHAIN_STATUS_DIAGNOSTICS_NOT_ACTIVE` when diagnostics are not active

#### Scenario: Host cannot retrieve events through callback registration
- **GIVEN** a runtime emits diagnostic events internally
- **WHEN** the host uses the public diagnostics boundary
- **THEN** no callback registration API SHALL be available for event delivery
- **AND** event-by-event retrieval SHALL remain outside the public contract

#### Scenario: Summary retrieval before preset load
- **GIVEN** a runtime in CREATED state with no preset loaded
- **WHEN** the host requests diagnostics through `goggles_fc_chain_get_diagnostic_summary(...)`
- **THEN** the boundary SHALL return a not-initialized status
- **AND** no event or artifact retrieval API SHALL be available to return partial or stale diagnostics data

### Requirement: No Public Capture Control Through Boundary API

The filter-chain boundary SHALL NOT expose capture control or other diagnostics expansion points as part of the current public contract.

#### Scenario: Current boundary exposes no capture requests
- **GIVEN** a host is using the current C boundary header
- **WHEN** it inspects the exported diagnostics functions
- **THEN** the implemented public diagnostics surface SHALL stop at `goggles_fc_chain_get_diagnostic_summary(...)`
- **AND** no public session lifecycle, sink registration, event retrieval, or capture request API SHALL be present

### Requirement: Boundary Diagnostic Event Emission Does Not Break Frame Recording Contract

Diagnostic event emission through the boundary SHALL NOT violate the existing frame recording performance contract.

#### Scenario: Tier 0 events during record
- **GIVEN** the filter-chain is recording commands for a frame
- **WHEN** Tier 0 diagnostic events are emitted
- **THEN** no heap allocation, file I/O, shader compilation, or blocking wait SHALL occur in the event emission path

#### Scenario: Tier 1 events with GPU timestamps
- **GIVEN** Tier 1 diagnostics are active during frame recording
- **WHEN** GPU timestamp queries are inserted for pass-level timing
- **THEN** timestamp query commands SHALL be recorded into the same command buffer
- **AND** timestamp readback SHALL occur asynchronously after frame submission, not during recording

### Requirement: Library-Owned Support Boundary

The extracted library SHALL own every public contract and every library-private support contract
required to build and verify itself outside the Goggles repository. Public headers and library-owned
internal code SHALL NOT depend on Goggles-private `src/util/*` headers, Goggles application config
types, or Goggles source-tree include-layout assumptions.

#### Scenario: Public surface excludes Goggles-private support headers
- **GIVEN** an external consumer compiles against the standalone library public headers
- **WHEN** header dependencies are audited from the install include root
- **THEN** the public surface SHALL depend only on boundary-owned declarations and allowed third-party headers
- **AND** no public header SHALL require Goggles-private `src/util/*` includes or Goggles app-private types

#### Scenario: Library-owned internals build without Goggles-private support
- **GIVEN** the standalone project builds its library sources from its own `src/` tree
- **WHEN** library-owned internal sources are compiled outside the Goggles repository
- **THEN** required support code SHALL be provided by the standalone project itself
- **AND** compilation SHALL NOT depend on Goggles-private helper ownership or Goggles checkout-relative include paths

### Requirement: Installed Public-Surface Verification Boundary

Reusable contract verification for the extracted library SHALL run against the installed public
surface using library-owned fixtures and assets. Goggles host integration coverage MAY verify host
wiring, but SHALL NOT replace installed-surface proof for the standalone library contract.

#### Scenario: Installed contract tests stay boundary-only
- **GIVEN** the standalone library has been installed to a test prefix
- **WHEN** contract tests compile and run against the installed headers, libraries, and package metadata
- **THEN** those tests SHALL validate boundary behavior without including Goggles-private source headers
- **AND** passing Goggles in-tree tests alone SHALL NOT satisfy standalone contract verification

#### Scenario: Library-owned fixtures and assets back verification
- **GIVEN** reusable contract tests exercise presets, shaders, or related runtime assets
- **WHEN** those tests run against the installed public surface
- **THEN** required fixtures and assets SHALL come from the library-owned project content
- **AND** the tests SHALL NOT depend on Goggles-owned fixture directories or shader asset paths

### Requirement: In-Repo Subdirectory Bridge During Extraction

The standalone filter-chain project SHALL initially reside as an in-repo subdirectory
(`filter-chain/`) within the Goggles repository. During the transitional extraction period,
Goggles SHALL consume the standalone target through `add_subdirectory(filter-chain/)` before
switching to `find_package()` consumption. The bridge period SHALL NOT introduce include-path
coupling back to the Goggles source tree.

#### Scenario: Transitional bridge keeps Goggles building

- **GIVEN** the standalone project skeleton exists at `filter-chain/` within the Goggles repository
- **WHEN** Goggles is configured with `add_subdirectory(filter-chain/)` as the integration path
- **THEN** the Goggles build SHALL succeed with all existing tests passing
- **AND** the standalone target SHALL expose only installed-surface include paths to the Goggles consumer

#### Scenario: Bridge does not re-introduce include-path coupling

- **GIVEN** Goggles consumes the standalone target through the `add_subdirectory()` bridge
- **WHEN** the standalone target's include directories are inspected
- **THEN** no include directory SHALL reference `${CMAKE_SOURCE_DIR}/src` or `${CMAKE_SOURCE_DIR}/src/render`
- **AND** Goggles code that compiles against the standalone target SHALL NOT resolve includes through Goggles source-tree layout assumptions

#### Scenario: Bridge is removed after package-first switch

- **GIVEN** Goggles has been switched to `find_package(GogglesFilterChain)` as the primary path
- **WHEN** the `add_subdirectory(filter-chain/)` bridge is inspected
- **THEN** the bridge MAY remain as an optional local-development convenience
- **AND** the bridge SHALL NOT be required for Goggles release acceptance

### Requirement: Standalone Source Tree Include-Path Isolation

All source files under the standalone project tree SHALL use standalone-relative include paths
exclusively. The standalone source tree SHALL NOT contain any include directives that assume
the Goggles source-tree layout.

#### Scenario: No Goggles util includes in standalone tree

- **GIVEN** all source and header files under `filter-chain/src/`
- **WHEN** include directives are audited (e.g., `grep -r '#include <util/' filter-chain/src/`)
- **THEN** zero matches SHALL be found for `#include <util/...>` paths
- **AND** zero matches SHALL be found for `#include <render/...>` paths that reference Goggles-layout modules

#### Scenario: Standalone internal includes use project-relative paths

- **GIVEN** implementation files under `filter-chain/src/`
- **WHEN** they include library-internal headers
- **THEN** include paths SHALL resolve through the standalone project's own include directories
- **AND** include paths SHALL NOT depend on Goggles `${CMAKE_SOURCE_DIR}/src` being on the include path

### Requirement: Library-Owned Support Shim Contracts

The standalone project SHALL provide library-owned support shims for logging, profiling, and
serialization that replace the Goggles `util/logging.hpp`, `util/profiling.hpp`, and
`util/serializer.hpp` dependencies. Each shim SHALL satisfy the same interface contract as the
Goggles original while depending only on the shim's own third-party dependency (spdlog for logging,
Tracy for profiling, standard library for serializer).

#### Scenario: Logging shim preserves tag-based facade contract

- **GIVEN** the standalone project's library-owned logging shim
- **WHEN** chain, shader, and texture implementation files use the logging facade
- **THEN** the shim SHALL support the same tagged logging macros (e.g., `GOGGLES_LOG_INFO`, `GOGGLES_LOG_WARN`)
- **AND** log tags (`render.chain`, `render.shader`, `render.texture`) SHALL be preserved
- **AND** the shim SHALL depend only on spdlog, not on Goggles `util/logging.hpp`

#### Scenario: Profiling shim compiles without Tracy when Tracy is unavailable

- **GIVEN** the standalone project is built without Tracy available in the build environment
- **WHEN** profiling macros are expanded in implementation files
- **THEN** profiling macros SHALL expand to no-op expressions
- **AND** compilation SHALL succeed without Tracy headers or libraries

#### Scenario: Serializer shim is self-contained

- **GIVEN** the standalone project's serializer shim under `filter-chain/src/support/`
- **WHEN** `shader_runtime.cpp` uses serialization utilities
- **THEN** the shim SHALL provide the required serialization interface using only the standard library
- **AND** the shim SHALL NOT include Goggles `util/serializer.hpp`
