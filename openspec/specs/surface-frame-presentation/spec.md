# surface-frame-presentation Specification

## Purpose
TBD - created by archiving change update-surface-frame-retention. Update Purpose after archive.
## Requirements
### Requirement: Preserve last surface frame on target change
The compositor presentation path SHALL retain the most recently presented frame for each surface
and use it when that surface becomes the active presentation target without waiting for a new
commit.

#### Scenario: Manual target switch
- **GIVEN** surface A and surface B have each presented at least one frame
- **AND** surface A is currently the presentation target
- **WHEN** the user sets surface B as the manual input target
- **THEN** the compositor SHALL present surface B's most recent retained frame without waiting for
  a new commit

#### Scenario: Auto focus switch
- **GIVEN** surface A and surface B have each presented at least one frame
- **AND** surface A is currently the presentation target
- **WHEN** focus changes to surface B via auto selection
- **THEN** the compositor SHALL present surface B's most recent retained frame without waiting for
  a new commit

### Requirement: Clear presentation when no retained frame exists
The compositor presentation path SHALL clear the presented frame when the new target surface has
no retained frame to display.

#### Scenario: Switch to a surface without a retained frame
- **GIVEN** surface A is currently presenting a frame
- **AND** surface B exists but has not yet presented a frame
- **WHEN** surface B becomes the presentation target
- **THEN** the compositor SHALL clear the presented frame

