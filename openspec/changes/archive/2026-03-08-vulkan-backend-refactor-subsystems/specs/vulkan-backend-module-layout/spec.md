## ADDED Requirements

### Requirement: VulkanBackend Facade Remains Stable

The backend refactor SHALL preserve `VulkanBackend` as the public integration facade for app-side
render backend usage.

The refactor SHALL:

- keep `src/render/backend/vulkan_backend.hpp` as the public API declaration surface
- preserve the existing public backend methods and `Application` integration points unless the
  change artifacts are updated first
- keep `src/render/backend/vulkan_backend.cpp` limited to public entrypoints and high-level
  orchestration after the split

#### Scenario: Public facade preserved after extraction
- **GIVEN** the backend refactor is complete
- **WHEN** app-side integration is inspected
- **THEN** `Application` still integrates through `VulkanBackend`
- **AND** the public backend declaration surface remains `src/render/backend/vulkan_backend.hpp`

### Requirement: Behavior-Oriented Backend Subsystems

The backend implementation SHALL organize internal code into behavior-oriented subsystems so future
edits can remain local to one backend concern.

The split SHALL provide subsystem boundaries that match these responsibilities:

- `VulkanContext` for instance, device, queue, surface, validation, and stable capability facts
- `RenderOutput` for swapchain/headless target lifetime and frame retirement
- `ExternalFrameImporter` for DMA-BUF import and temporary explicit-sync wait ownership
- `FilterChainController` for backend-side boundary-facing filter coordination, reload requests,
  policy inputs, current preset path state, and temporary GPU-drain-safe retirement

Long-lived filter runtime ownership, shader runtime ownership/creation, shader processing, and
preset texture loading SHALL remain inside `goggles-filter-chain`.

The implementation SHALL NOT introduce a generic `misc`, `helpers`, or `utils` dumping-ground module
for backend extraction.

#### Scenario: Localized edit surface for output behavior
- **GIVEN** a future change only affects swapchain recreation, headless target lifetime, or frame
  retirement behavior
- **WHEN** an implementer identifies the primary edit surface
- **THEN** the primary implementation surface is `RenderOutput`
- **AND** unrelated importer and filter-controller logic does not need to remain in the same
  translation unit

#### Scenario: Localized edit surface for import behavior
- **GIVEN** a future change only affects imported image lifetime or explicit-sync import behavior
- **WHEN** an implementer identifies the primary edit surface
- **THEN** the primary implementation surface is `ExternalFrameImporter`
- **AND** output-target and filter-runtime logic does not need to remain in the same translation unit

### Requirement: Filter Boundary Ownership Remains Intact

The backend refactor SHALL keep long-lived filter runtime ownership inside `goggles-filter-chain`
while using backend-side coordination only for boundary-facing sequencing and temporary retirement.

#### Scenario: Runtime ownership stays in the filter boundary
- **GIVEN** the renderer initializes or reloads filter processing after the backend split
- **WHEN** runtime ownership is inspected
- **THEN** chain orchestration, shader runtime ownership/creation, shader processing, and preset
  texture loading remain owned within `goggles-filter-chain`
- **AND** backend-side `FilterChainController` consumes boundary-facing contracts instead of owning
  those long-lived runtime internals

#### Scenario: Host-side retirement remains bounded
- **GIVEN** a filter runtime handoff replaces an active runtime after the backend split
- **WHEN** the previous runtime is retired
- **THEN** any host-side retirement ownership is temporary and limited to GPU-drain-safe destruction
- **AND** active runtime ownership remains in the filter boundary

### Requirement: Subsystem Authority and Dependency Direction Remain Explicit

The backend refactor SHALL preserve one explicit authority for each mutable backend state domain and
SHALL keep subsystem dependency direction acyclic.

The split SHALL enforce these authority rules:

- `VulkanBackend` coordinates cross-subsystem transitions
- `RenderOutput` is the authority for target extent, target format, and frame-retirement state
- `ExternalFrameImporter` is the authority for imported-image lifetime and temporary wait objects
- `FilterChainController` is the authority for backend-side reload request state, stage-policy input
  state, current preset path state, and temporary host-side retirement bookkeeping

Allowed internal dependency edges are:

- `RenderOutput -> VulkanContext`
- `ExternalFrameImporter -> VulkanContext`
- `FilterChainController -> boundary-owned host<->filter VulkanContext contract`

Forbidden internal dependency edges are:

- `RenderOutput -/-> ExternalFrameImporter`
- `RenderOutput -/-> FilterChainController`
- `ExternalFrameImporter -/-> FilterChainController`

#### Scenario: Single owner remains inspectable after extraction
- **GIVEN** the backend implementation has been split into subsystem-oriented files
- **WHEN** target state, imported-source state, and filter-runtime state ownership are inspected
- **THEN** each mutable state domain still resolves to one named subsystem owner
- **AND** `VulkanBackend` does not duplicate that authoritative state without an explicitly documented reason

#### Scenario: Cross-subsystem calls stay facade-owned
- **GIVEN** the backend implementation has been split into subsystem-oriented files
- **WHEN** a frame is rendered or the swapchain is recreated
- **THEN** cross-boundary sequencing is coordinated by `VulkanBackend`
- **AND** non-context subsystems do not call each other directly to trigger rebuild or lifetime transitions

### Requirement: Boundary-Owned VulkanContext Contract Remains Intact

The backend refactor SHALL preserve a boundary-owned host<->filter `VulkanContext` contract so the
filter boundary does not consume backend-only context headers.

#### Scenario: Filter boundary uses a boundary-owned context contract
- **GIVEN** host/backend code initializes `goggles-filter-chain` after the backend split
- **WHEN** include and initialization dependencies are inspected
- **THEN** the filter boundary consumes a boundary-owned host<->filter `VulkanContext` contract or adapter
- **AND** backend-only `VulkanContext` headers are not included directly from filter-boundary code

### Requirement: Extraction Contract Is Explicit for Apply

The change artifacts SHALL define a compile-safe extraction order and verification plan that minimize
behavior drift during apply.

The change artifacts SHALL specify:

- an initial declaration-seam step that adds only the narrow internal headers and types needed for a
  buildable multi-file split
- updating `src/render/backend/CMakeLists.txt` as backend translation units land
- extracting `VulkanContext` before other subsystem owners
- extracting `RenderOutput` before `ExternalFrameImporter` and `FilterChainController`
- extracting `ExternalFrameImporter` before final facade cleanup so imported-source ownership is explicit
- preserving or introducing the boundary-owned host<->filter `VulkanContext` contract before
  `FilterChainController` depends on it
- extracting `FilterChainController` after output/import seams are stable
- a verification contract that names baseline preset gates, environment-sensitive checks, fallback
  policy, mandatory no-fallback checks, forbidden dependency-edge audits, and named module-layout
  plus lifetime tests

#### Scenario: Migration order is explicit from artifacts alone
- **GIVEN** implementation starts from the repository artifacts alone
- **WHEN** the artifacts are read before editing code
- **THEN** the OpenSpec artifacts specify the required extraction order
- **AND** the implementation does not depend on undocumented external context to determine safe sequencing

### Requirement: Backend Behavior Is Preserved Across the Split

The backend refactor SHALL preserve existing backend behavior while changing only the internal module
layout.

The preserved behavior SHALL include:

- windowed swapchain rendering and present flow
- headless surfaceless rendering and PNG readback behavior
- DMA-BUF plane-layout import and explicit-sync submission wiring
- filter-chain stage-order invariants and async preset reload behavior
- Result-based error propagation and no expected-failure exceptions in backend refactor scope
- `goggles::util::JobSystem`-owned async preset rebuild behavior
- filter-boundary ownership of long-lived runtime objects and boundary-owned initialization contracts
- swapchain recreation behavior, including output-format and filter-runtime rebuild coordination
- unchanged `Application` backend integration points

#### Scenario: Headless and windowed behavior remain unchanged
- **GIVEN** the backend implementation has been split into subsystem-oriented files
- **WHEN** windowed and headless render flows are exercised through the defined verification plan
- **THEN** swapchain render/present behavior and headless offscreen/readback behavior remain unchanged

#### Scenario: DMA-BUF and filter-chain behavior remain unchanged
- **GIVEN** the backend implementation has been split into subsystem-oriented files
- **WHEN** imported-frame, explicit-sync, and filter-boundary behavior are exercised through the defined verification plan
- **THEN** DMA-BUF import semantics, temporary wait handling, stage ordering, and async preset reload behavior remain unchanged

#### Scenario: Error flow and async rebuild behavior remain unchanged
- **GIVEN** the backend implementation has been split into subsystem-oriented files
- **WHEN** backend failure paths and preset reload behavior are exercised through the defined verification plan
- **THEN** expected runtime failures still propagate through `Result`-style APIs without expected-failure exceptions
- **AND** async preset rebuild behavior remains owned by `goggles::util::JobSystem`

### Requirement: Shutdown and Async Lifetime Ordering Remain Safe

The backend refactor SHALL preserve auditable shutdown ordering across async filter work, imported
resources, output resources, and Vulkan context state.

The shutdown contract SHALL require this order:

- wait for pending async filter rebuild work and clear pending-ready state
- idle the device before subsystem teardown proceeds
- destroy filter-controller active, pending, and deferred-retire runtime state before output/context teardown
- destroy imported-image state before output/context teardown completes
- destroy output resources before destroying context-owned device, surface, debug messenger, and instance state
- treat any shutdown or async lifetime ordering drift as a proposal/spec/design reconciliation stop condition

#### Scenario: Device-rooted resources tear down in safe order
- **GIVEN** the backend implementation is shutting down after the subsystem split
- **WHEN** teardown order is inspected or exercised through verification
- **THEN** async runtime work is resolved before subsystem destruction proceeds
- **AND** device-rooted child resources are destroyed before the owning Vulkan context state is destroyed
