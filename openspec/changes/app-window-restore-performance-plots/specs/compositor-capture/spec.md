## MODIFIED Requirements

### Requirement: Compositor Capture Publishes Gameplay Metrics

The compositor capture path SHALL publish the timing data required for the Application performance
panel to report and plot `Game FPS` and `Compositor Latency`.

The runtime metrics contract SHALL include current scalar values plus bounded history for both
metrics for the current capture target.

`Game FPS` current value and published history SHALL be derived from presents or commits for the
currently captured game surface only.

`Compositor Latency` current value and published history SHALL be derived from the interval between
an eligible active-surface commit and the corresponding compositor capture publication.

When the current capture target changes, the published metric history SHALL reset before accumulating
samples for the new target.

#### Scenario: Active surface commit updates Game FPS source and history
- **GIVEN** a game surface is the current capture target
- **WHEN** that surface produces an eligible commit for capture
- **THEN** the compositor capture path SHALL update the `Game FPS` metric source from that event
- **AND** it SHALL update the published `Game FPS` history for that capture target

#### Scenario: Non-target surface does not change Game FPS source or history
- **GIVEN** a different surface is not the current capture target
- **WHEN** that non-target surface commits
- **THEN** the compositor capture path SHALL NOT count that event toward `Game FPS`
- **AND** it SHALL NOT update the published `Game FPS` history for the current target

#### Scenario: Commit-to-capture latency is published with bounded history
- **GIVEN** an eligible active-surface commit produces a captured frame
- **WHEN** the compositor publishes the captured frame for viewer consumption
- **THEN** the compositor capture path SHALL publish `Compositor Latency` for that commit as the
  elapsed commit-to-capture interval
- **AND** it SHALL update the published bounded latency history for the current target

#### Scenario: Capture target change clears published histories
- **GIVEN** runtime metric history exists for one capture target
- **WHEN** the compositor switches to a different capture target
- **THEN** the published `Game FPS` and `Compositor Latency` histories SHALL reset before samples for
  the new target are exposed
