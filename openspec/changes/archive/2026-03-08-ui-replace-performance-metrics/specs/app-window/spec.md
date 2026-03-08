## ADDED Requirements

### Requirement: Application Performance Panel Reports Gamer-Facing Metrics

The Application performance panel SHALL report `Game FPS` and `Compositor Latency` instead of the
legacy `Render` and `Source` FPS metrics.

The panel SHALL display compositor-provided `Game FPS` and `Compositor Latency` values.

#### Scenario: Performance panel shows replacement metrics
- **WHEN** the Application performance panel is rendered
- **THEN** it SHALL display `Game FPS` and `Compositor Latency`
- **AND** it SHALL NOT display `Render` FPS or `Source` FPS

#### Scenario: Legacy performance plots are removed
- **WHEN** the Application performance panel is rendered after this change
- **THEN** it SHALL NOT render the legacy frame-history plots associated with `Render` and `Source`
  FPS

#### Scenario: Game FPS follows active captured game surface only
- **GIVEN** a game surface is the current capture target
- **WHEN** the performance panel reports `Game FPS`
- **THEN** the reported value SHALL come from the compositor-provided metric snapshot for that
  capture target only
