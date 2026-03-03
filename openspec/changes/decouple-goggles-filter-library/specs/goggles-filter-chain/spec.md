## ADDED Requirements

### Requirement: Standalone Filter Library Target
The extracted filter runtime SHALL build as a standalone target named `goggles-filter-chain` with one-way dependency direction between host backend and filter boundary.

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

### Requirement: Complete Filter Runtime Ownership Boundary
The filter boundary SHALL own filter-chain orchestration, shader runtime ownership/creation, shader processing, and preset texture loading internals.

#### Scenario: Runtime ownership initialization
- **GIVEN** the renderer initializes filter processing
- **WHEN** filter runtime components are created
- **THEN** chain orchestration, shader runtime, and preset texture loading SHALL be created and owned within `goggles-filter-chain`
- **AND** host backend code SHALL consume runtime behavior through boundary-facing contracts

#### Scenario: Shader and texture wiring ownership
- **GIVEN** build wiring for chain/shader/texture modules
- **WHEN** `goggles-filter-chain` is configured
- **THEN** shader and texture module wiring SHALL be included under the `goggles-filter-chain` ownership boundary
- **AND** host backend code SHALL NOT own shader-runtime creation paths directly

### Requirement: Host Backend Responsibility Boundary
The host backend SHALL remain responsible for swapchain lifecycle, external image import, synchronization, queue submission, and present.

#### Scenario: Frame submission boundary
- **GIVEN** the host backend submits a frame context for filtering
- **WHEN** filter processing commands are recorded
- **THEN** `goggles-filter-chain` SHALL record filter-processing work from caller-provided frame context
- **AND** the host backend SHALL remain responsible for present-path submission and display ownership

#### Scenario: Resize and recreation handoff
- **GIVEN** swapchain extent or format changes require recreation
- **WHEN** render resources are recreated
- **THEN** host backend code SHALL drive swapchain and present resource recreation
- **AND** filter runtime resize/recreation SHALL be invoked through boundary-facing contracts

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
