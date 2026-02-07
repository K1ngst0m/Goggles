## ADDED Requirements

### Requirement: Dynamic Scale Mode

The viewer SHALL support a dynamic scale mode that requests source resolution changes to match the viewer window.

#### Scenario: Dynamic mode activation

- **GIVEN** `scale_mode = "dynamic"` in configuration
- **WHEN** the viewer window is resized
- **THEN** the viewer SHALL send a resolution request to the source
- **AND** render using fit mode until source resolution changes

#### Scenario: Dynamic mode with WSI proxy source

- **GIVEN** `scale_mode = "dynamic"` is configured
- **AND** source is running in WSI proxy mode
- **WHEN** the viewer window is resized
- **THEN** the source SHALL recreate its swapchain with the new resolution
- **AND** subsequent frames SHALL match the viewer window resolution

#### Scenario: Dynamic mode with non-proxy source

- **GIVEN** `scale_mode = "dynamic"` is configured
- **AND** source is NOT running in WSI proxy mode
- **WHEN** the viewer window is resized
- **THEN** the resolution request SHALL be ignored by the source
- **AND** the viewer SHALL fall back to fit mode behavior