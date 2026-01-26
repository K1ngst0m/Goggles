## ADDED Requirements
### Requirement: Compositor Public API

The system SHALL expose input forwarding and compositor-presented surface frames via the `CompositorServer` public API, without requiring a separate forwarding wrapper.

#### Scenario: Application integrates compositor directly
- **WHEN** the application initializes input forwarding
- **THEN** it creates and owns a `CompositorServer`
- **AND** it forwards SDL input events via `CompositorServer` methods
- **AND** it can query compositor-presented surface frames from the same instance
