## MODIFIED Requirements

### Requirement: Application Performance Panel Reports Gamer-Facing Metrics

The Application performance panel SHALL report `Game FPS` and `Compositor Latency` instead of the
legacy `Render` and `Source` FPS metrics.

The panel SHALL display compositor-provided current `Game FPS` and `Compositor Latency` values.

The panel SHALL also render one live historical plot for each metric directly beneath the
corresponding text readout using compositor-provided bounded history for the current capture target.

#### Scenario: Performance panel shows replacement metrics
- **WHEN** the Application performance panel is rendered
- **THEN** it SHALL display `Game FPS` and `Compositor Latency`
- **AND** it SHALL NOT display `Render` FPS or `Source` FPS

#### Scenario: Performance panel renders both metric plots
- **GIVEN** compositor-provided history is available for the current capture target
- **WHEN** the Application performance panel is rendered
- **THEN** it SHALL render one live `Game FPS` plot directly beneath the `Game FPS` text readout
- **AND** it SHALL render one live `Compositor Latency` plot directly beneath the latency text
  readout

#### Scenario: Performance panel does not recreate legacy timing state
- **WHEN** the Application performance panel renders metric plots
- **THEN** it SHALL consume compositor-provided metric history
- **AND** it SHALL NOT reintroduce legacy `Render` / `Source` timing buffers or plots

#### Scenario: Game FPS follows active captured game surface only
- **GIVEN** a game surface is the current capture target
- **WHEN** the performance panel reports or plots `Game FPS`
- **THEN** the displayed value and plotted history SHALL come from the compositor-provided metric
  snapshot for that capture target only
