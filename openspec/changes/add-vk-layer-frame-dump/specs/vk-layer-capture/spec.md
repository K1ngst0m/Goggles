## ADDED Requirements

### Requirement: Present Frame Dump Directory

The dump output directory SHALL be configurable via `GOGGLES_DUMP_DIR`.

#### Scenario: Default dump directory

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is set to a non-empty value
- **AND** `GOGGLES_DUMP_DIR` is not set or is an empty string
- **WHEN** a selected frame is dumped
- **THEN** the layer SHALL write dumps under `/tmp/goggles_dump`

#### Scenario: Custom dump directory

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is set to a non-empty value
- **AND** `GOGGLES_DUMP_DIR` is set to a non-empty value
- **WHEN** a selected frame is dumped
- **THEN** the layer SHALL write dumps under that directory

### Requirement: Present Frame Dump Configuration

The capture layer SHALL support dumping the presented image to disk when
`GOGGLES_DUMP_FRAME_RANGE` is set to a non-empty value.

#### Scenario: Dumping disabled by default

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is not set or is an empty string
- **WHEN** `vkQueuePresentKHR` is called
- **THEN** the layer SHALL NOT dump any images to disk

#### Scenario: Dumping enabled when range is set

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is set to a non-empty value
- **WHEN** a frame is presented whose frame number matches the range
- **THEN** the layer SHALL dump that presented image to disk

### Requirement: Present Frame Dump Outputs

When dumping a selected frame, the layer SHALL write both:
- a `ppm` image file
- a metadata sidecar file with `.ppm.desc` suffix

Both files SHALL share the same base name `{processname}_{frameid}` and be written to the same
directory.

For dumping, `{frameid}` SHALL be the frame number used for capture metadata
(`CaptureFrameMetadata.frame_number`) for that frame.

#### Scenario: Image and metadata outputs are produced

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE=3`
- **AND** the process name is `vkcube`
- **WHEN** frame number 3 is dumped
- **THEN** the layer SHALL write `vkcube_3.ppm`
- **AND** it SHALL write `vkcube_3.ppm.desc`

### Requirement: Present Frame Dump Range Syntax

`GOGGLES_DUMP_FRAME_RANGE` SHALL support selecting frames using a union of:
- single frame numbers (e.g. `3`)
- comma-separated lists (e.g. `3,5,8`)
- inclusive ranges (e.g. `8-13`)

Whitespace around commas and hyphens MAY be ignored.

For the purpose of `GOGGLES_DUMP_FRAME_RANGE`, the “frame number” SHALL be the value used by the
layer for capture metadata (`CaptureFrameMetadata.frame_number`) for that frame.

#### Scenario: Single frame selection

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE=3`
- **WHEN** the layer observes frame numbers 1, 2, 3, 4
- **THEN** it SHALL dump frame 3
- **AND** it SHALL NOT dump frames 1, 2, or 4

#### Scenario: Multi-frame list selection

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE=3,5,8`
- **WHEN** the layer observes frame numbers 1 through 9
- **THEN** it SHALL dump frames 3, 5, and 8
- **AND** it SHALL NOT dump other frames

#### Scenario: Inclusive range selection

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE=8-13`
- **WHEN** the layer observes frame numbers 1 through 14
- **THEN** it SHALL dump frames 8 through 13 (inclusive)
- **AND** it SHALL NOT dump frames 7 or 14

### Requirement: Present Frame Dump Metadata Format

The `.ppm.desc` file SHALL be plain text with one `key=value` pair per line to avoid requiring
additional serialization dependencies.

#### Scenario: Metadata includes core fields

- **GIVEN** a frame is dumped
- **WHEN** the layer writes the `.desc` file
- **THEN** it SHALL include a `frame_number` key
- **AND** it SHALL include a `width` key
- **AND** it SHALL include a `height` key
- **AND** it SHALL include a `format` key
- **AND** it SHALL include a `stride` key
- **AND** it SHALL include an `offset` key
- **AND** it SHALL include a `modifier` key

### Requirement: Present Frame Dump Mode

The dump output format SHALL be controlled by `GOGGLES_DUMP_FRAME_MODE`.

#### Scenario: Default dump mode

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is set
- **AND** `GOGGLES_DUMP_FRAME_MODE` is not set or is an empty string
- **WHEN** a selected frame is dumped
- **THEN** the layer SHALL write a `ppm` dump

#### Scenario: Unsupported dump mode fallback

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is set
- **AND** `GOGGLES_DUMP_FRAME_MODE` is set to an unsupported value
- **WHEN** a selected frame is dumped
- **THEN** the layer SHALL fall back to `ppm`

### Requirement: Present Frame Dump Compatibility

Present frame dumping SHALL work in both WSI proxy and non-WSI-proxy capture paths.

#### Scenario: Dumping in WSI proxy mode

- **GIVEN** WSI proxy mode is enabled (`GOGGLES_WSI_PROXY=1` and `GOGGLES_CAPTURE=1`)
- **AND** `GOGGLES_DUMP_FRAME_RANGE` selects a frame number
- **WHEN** the application presents a virtual swapchain image
- **THEN** the layer SHALL dump the presented image to disk

#### Scenario: Dumping in non-WSI-proxy mode

- **GIVEN** WSI proxy mode is disabled
- **AND** `GOGGLES_DUMP_FRAME_RANGE` selects a frame number
- **WHEN** the application presents a real swapchain image
- **THEN** the layer SHALL dump the presented image to disk

### Requirement: Present Frame Dump Performance Constraints

Dumping SHALL be implemented such that `vkQueuePresentKHR` does not perform file I/O or logging.

#### Scenario: No file I/O or logging on present hook

- **GIVEN** `GOGGLES_DUMP_FRAME_RANGE` is set to a non-empty value
- **WHEN** `vkQueuePresentKHR` is called
- **THEN** the layer SHALL NOT perform file I/O on the present hook thread
- **AND** it SHALL NOT log on the present hook thread
- **AND** any disk writes SHALL be performed asynchronously (off-thread)
