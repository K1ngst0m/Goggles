# object-lifecycle Spec Delta

## ADDED Requirements

### Requirement: App Orchestrator Uses Factory Pattern

The app orchestrator component (e.g., `goggles::app::Application`) SHALL use the factory pattern (`create(...) -> ResultPtr<T>`) for fallible initialization and SHALL NOT expose two-phase initialization.

#### Scenario: Orchestrator initialization failure
- **WHEN** orchestrator initialization fails at any step
- **THEN** `create(...)` SHALL return an error `ResultPtr`
- **AND** any partially acquired resources SHALL be cleaned up automatically via RAII

## MODIFIED Requirements

### Requirement: Classes Using Factory Pattern

The following classes SHALL use factory pattern instead of two-phase initialization:
- VulkanBackend
- FilterChain
- FilterPass
- ShaderRuntime
- Framebuffer
- OutputPass
- CaptureReceiver
- InputForwarder (already implemented)
- Application (app orchestrator)

#### Scenario: Factory method signature
- **WHEN** implementing a factory method
- **THEN** it SHALL be declared as `[[nodiscard]] static auto create(...) -> ResultPtr<ClassName>;`
- **AND** it SHALL accept all necessary initialization parameters
- **AND** it SHALL use `GOGGLES_TRY()` for nested factory calls where applicable

