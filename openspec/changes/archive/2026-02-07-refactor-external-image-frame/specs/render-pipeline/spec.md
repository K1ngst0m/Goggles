## ADDED Requirements
### Requirement: External Image Format Normalization
The application SHALL represent external image metadata using `VkFormat` for all render imports.
Sources that provide DRM FourCC formats (e.g., compositor surface frames) SHALL be converted to
`VkFormat` before reaching the render backend.

#### Scenario: Compositor surface frame conversion
- **GIVEN** a compositor frame provides DRM FourCC format metadata
- **WHEN** the frame is ingested by the application
- **THEN** the metadata SHALL be converted to the equivalent `VkFormat`
- **AND** frames with unsupported formats SHALL be skipped

#### Scenario: Capture receiver format passthrough
- **GIVEN** a capture frame provides `VkFormat` metadata over IPC
- **WHEN** the application ingests the frame
- **THEN** the metadata SHALL be forwarded unchanged to the render backend
