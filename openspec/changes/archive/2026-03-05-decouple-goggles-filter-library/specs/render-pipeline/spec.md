## MODIFIED Requirements

### Requirement: Pipeline Extensibility

The render architecture SHALL support future multi-pass shader processing through a modular structure with explicit host-backend and filter-library boundaries.

#### Scenario: Module organization

- **GIVEN** the render module structure
- **WHEN** pipeline responsibilities are assigned
- **THEN** host backend code SHALL own swapchain, external image import, synchronization, and present
- **AND** the Goggles filter library boundary SHALL own filter-chain orchestration, shader processing, and preset texture loading internals
- **AND** app-facing filter operations SHALL be accessed via backend-facing facade methods rather than exposing concrete chain types

#### Scenario: Stage ordering invariance

- **GIVEN** the filter runtime executes pre-chain, effect, and post-chain stages
- **WHEN** filter boundary extraction changes ownership and call paths
- **THEN** stage execution order SHALL remain pre-chain -> effect -> post-chain

#### Scenario: Semantic binding invariance

- **GIVEN** shaders relying on established semantic texture bindings
- **WHEN** filter processing runs after boundary extraction
- **THEN** semantic bindings SHALL remain unchanged for `Source`, `OriginalHistory#`, `PassOutput#`, `PassFeedback#`, and `<alias>Feedback`

#### Scenario: Error handling macros

- **GIVEN** Vulkan API calls that return `vk::Result`
- **WHEN** error checking is needed
- **THEN** `VK_TRY(call, code, msg)` macro SHALL be used for early return
- **AND** error message SHALL include the Vulkan result string

#### Scenario: Result propagation

- **GIVEN** internal functions that return `Result<T>`
- **WHEN** the result needs to be propagated to the caller
- **THEN** `GOGGLES_TRY(expr)` macro SHALL be used for early return

## ADDED Requirements

### Requirement: Async Filter Lifecycle Safety

The render pipeline SHALL preserve async preset reload, chain swap, and resize safety behavior after introducing the `goggles-filter-chain` boundary.

#### Scenario: Async reload and swap notification ordering

- **GIVEN** a preset reload is executed asynchronously
- **WHEN** the new chain becomes active for rendering
- **THEN** swap notification SHALL be emitted only after activation of the new chain
- **AND** consumers of swap notification SHALL observe the new chain as active state

#### Scenario: Chain-swapped signal observability

- **GIVEN** an async reload completes and activates a new chain/runtime
- **WHEN** host code consumes the chain-swapped signal
- **THEN** the signal SHALL indicate swap completion only after the new chain/runtime is active
- **AND** the first successful consumption SHALL clear the current swap indication
- **AND** additional consumption attempts before the next successful activation SHALL report no swap-complete indication

#### Scenario: Async reload activation failure behavior

- **GIVEN** an async reload attempt fails before activating a replacement chain/runtime
- **WHEN** host code checks active runtime state and chain-swapped signal state
- **THEN** the previously active chain/runtime SHALL remain active for rendering
- **AND** the chain-swapped signal SHALL remain unset and SHALL NOT report swap completion
- **AND** repeated signal consumption attempts before the next successful activation SHALL continue reporting no swap-complete indication

#### Scenario: Deferred destroy safety after chain swap

- **GIVEN** a chain swap replaces active filter runtime objects
- **WHEN** previous runtime resources are retired
- **THEN** retired resources SHALL be deferred until safe destruction criteria are met
- **AND** rendering SHALL NOT access retired chain resources after retirement begins
- **AND** active runtime ownership SHALL remain in the filter boundary while host-side retirement ownership is temporary and limited to GPU-drain-safe destruction

#### Scenario: Resize handoff with boundary split

- **GIVEN** resize or format changes trigger swapchain recreation
- **WHEN** host backend recreates present-path resources
- **THEN** filter runtime resize/recreation SHALL occur through boundary-facing operations
- **AND** app and UI modules SHALL NOT directly recreate concrete chain resources
