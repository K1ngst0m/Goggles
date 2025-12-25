## ADDED Requirements

### Requirement: Preset Reference Directive

The preset parser SHALL support `#reference` directive for including other presets.

#### Scenario: Mega-Bezel modular preset
- **GIVEN** a preset contains `#reference "Base_CRT_Presets/MBZ__3__STD__GDV.slangp"`
- **WHEN** the preset is parsed
- **THEN** the referenced preset SHALL be loaded and merged
- **AND** paths SHALL be resolved relative to the referencing file

#### Scenario: Nested references
- **GIVEN** preset A references preset B which references preset C
- **WHEN** parsing completes
- **THEN** all references SHALL be resolved recursively
- **AND** final config SHALL contain merged settings from all presets

#### Scenario: Reference depth limit
- **GIVEN** a reference chain exceeds 8 levels
- **WHEN** parsing is attempted
- **THEN** an error SHALL be returned with message indicating depth exceeded

#### Scenario: Circular reference detection
- **GIVEN** preset A references preset B which references preset A
- **WHEN** parsing is attempted
- **THEN** an error SHALL be returned indicating circular reference

### Requirement: Frame History Access

The filter chain SHALL maintain a ring buffer of previous frame textures and expose them as OriginalHistory[0-6] samplers.

#### Scenario: Afterglow accesses previous frame
- **GIVEN** a shader samples `OriginalHistory0`
- **WHEN** the pass is recorded
- **THEN** `OriginalHistory0` SHALL be bound to the previous frame's Original texture
- **AND** `OriginalHistory0Size` SHALL be populated as vec4 [width, height, 1/width, 1/height]

#### Scenario: Motion interpolation accesses multiple frames
- **GIVEN** a shader samples `OriginalHistory1` and `OriginalHistory2`
- **WHEN** the pass is recorded
- **THEN** `OriginalHistory1` SHALL be bound to frame N-2
- **AND** `OriginalHistory2` SHALL be bound to frame N-3

#### Scenario: History depth auto-detection
- **GIVEN** shaders reference `OriginalHistory3` as highest index
- **WHEN** filter chain initializes
- **THEN** a ring buffer of exactly 4 frames SHALL be allocated
- **AND** unused history slots SHALL NOT be allocated

#### Scenario: First frames without history
- **GIVEN** filter chain has processed fewer frames than history depth
- **WHEN** OriginalHistory[N] is requested for unavailable frame
- **THEN** a black texture SHALL be bound as fallback

### Requirement: Frame Count Modulo

The filter chain SHALL apply per-pass frame_count_mod to the FrameCount semantic.

#### Scenario: NTSC alternating lines
- **GIVEN** pass 27 sets `frame_count_mod27 = 2`
- **AND** current absolute frame is 157
- **WHEN** FrameCount semantic is populated for pass 27
- **THEN** FrameCount SHALL be 157 % 2 = 1

#### Scenario: Different modulo per pass
- **GIVEN** pass 5 sets `frame_count_mod5 = 4`
- **AND** pass 10 sets `frame_count_mod10 = 100`
- **WHEN** passes are recorded
- **THEN** each pass SHALL receive its own modulo-applied FrameCount

#### Scenario: No modulo uses absolute count
- **GIVEN** no frame_count_mod is set for a pass
- **WHEN** FrameCount semantic is populated
- **THEN** FrameCount SHALL be the absolute frame count

#### Scenario: Modulo value of zero
- **GIVEN** `frame_count_mod5 = 0` is set
- **WHEN** FrameCount semantic is populated for pass 5
- **THEN** FrameCount SHALL be the absolute frame count (0 means disabled)

### Requirement: Rotation Semantic

The semantic binder SHALL provide Rotation push constant for display orientation.

#### Scenario: No rotation (landscape)
- **GIVEN** display rotation is 0 degrees
- **WHEN** Rotation semantic is populated
- **THEN** Rotation SHALL be 0

#### Scenario: Portrait rotation (90 degrees)
- **GIVEN** display rotation is 90 degrees clockwise
- **WHEN** Rotation semantic is populated
- **THEN** Rotation SHALL be 1

#### Scenario: Inverted rotation (180 degrees)
- **GIVEN** display rotation is 180 degrees
- **WHEN** Rotation semantic is populated
- **THEN** Rotation SHALL be 2

#### Scenario: Portrait rotation (270 degrees)
- **GIVEN** display rotation is 270 degrees clockwise
- **WHEN** Rotation semantic is populated
- **THEN** Rotation SHALL be 3