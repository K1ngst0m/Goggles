## ADDED Requirements

### Requirement: Compositor Capture Publishes Gameplay Metrics

The compositor capture path SHALL publish the timing data required for the Application performance
panel to report `Game FPS` and `Compositor Latency`.

`Game FPS` SHALL be derived from presents or commits for the currently captured game surface only.
`Compositor Latency` SHALL be derived from the interval between an eligible active-surface commit
and the corresponding compositor capture publication.

#### Scenario: Active surface commit updates Game FPS source
- **GIVEN** a game surface is the current capture target
- **WHEN** that surface produces an eligible commit for capture
- **THEN** the compositor capture path SHALL update the `Game FPS` metric source from that event

#### Scenario: Non-target surface does not change Game FPS source
- **GIVEN** a different surface is not the current capture target
- **WHEN** that non-target surface commits
- **THEN** the compositor capture path SHALL NOT count that event toward `Game FPS`

#### Scenario: Commit-to-capture latency is published
- **GIVEN** an eligible active-surface commit produces a captured frame
- **WHEN** the compositor publishes the captured frame for viewer consumption
- **THEN** the compositor capture path SHALL publish `Compositor Latency` for that commit as the
  elapsed commit-to-capture interval
