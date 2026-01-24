# Spec Delta: render-pipeline

## MODIFIED Requirements

### Requirement: Pass Infrastructure

The render chain subsystem SHALL provide a Pass abstraction compatible with RetroArch shader system.

#### Scenario: Pass interface

- **GIVEN** a Pass implementation
- **WHEN** initialized with device, target format, num_sync_indices, and shader runtime
- **THEN** the pass SHALL create its pipeline with `VkPipelineRenderingCreateInfo`
- **AND** allocate `num_sync_indices` descriptor sets from its pool

#### Scenario: PassContext provides rendering target

- **GIVEN** a PassContext for recording
- **WHEN** passed to `Pass::record()`
- **THEN** it SHALL contain `target_image_view` (swapchain or intermediate image view)
- **AND** it SHALL contain `target_format` (for barrier transitions)
- **AND** it SHALL contain `source_texture` (previous pass output)
- **AND** it SHALL contain `original_texture` (normalized input)
- **AND** it SHALL contain `frame_index` for descriptor set selection
- **AND** it SHALL contain `output_extent` for viewport/scissor setup

#### Scenario: Per-frame descriptor isolation

- **GIVEN** num_sync_indices = 2
- **WHEN** frame N is recording while frame N-1 is still on GPU
- **THEN** the pass SHALL update `descriptor_sets[N % 2]`
- **AND** NOT touch `descriptor_sets[(N-1) % 2]`
- **AND** no validation error SHALL occur

## ADDED Requirements

### Requirement: Pass Shader Parameter Interface

The `Pass` base class SHALL provide virtual methods for exposing tunable shader uniforms, allowing internal passes to opt-in to runtime parameter adjustment.

#### Scenario: Default implementation returns no parameters

- **GIVEN** a Pass subclass that does not override parameter methods
- **WHEN** `get_shader_parameters()` is called
- **THEN** an empty vector SHALL be returned

#### Scenario: Pass exposes shader parameters

- **GIVEN** a Pass subclass that overrides `get_shader_parameters()`
- **WHEN** the method is called
- **THEN** a vector of `ShaderParameter` metadata SHALL be returned
- **AND** each entry SHALL include name, description, default, min, max, and step

#### Scenario: Parameter value update

- **GIVEN** a Pass with exposed parameters
- **WHEN** `set_shader_parameter(name, value)` is called
- **THEN** the internal parameter value SHALL be updated
- **AND** the change SHALL take effect on the next frame

#### Scenario: FilterPass implements parameter interface

- **GIVEN** a FilterPass with shader parameters from preprocessing
- **WHEN** `get_shader_parameters()` is called
- **THEN** it SHALL return the parameters extracted from the shader
- **AND** `set_shader_parameter()` SHALL update the parameter override map

### Requirement: Downsample Filter Type Selection

The DownsamplePass SHALL support runtime selection of downsampling filter algorithm via the shader parameter interface.

#### Scenario: Area filter (default)

- **GIVEN** DownsamplePass with filter_type = 0
- **WHEN** downsampling is performed
- **THEN** weighted box filter SHALL be used
- **AND** each source pixel SHALL be weighted by coverage overlap

#### Scenario: Gaussian filter

- **GIVEN** DownsamplePass with filter_type = 1
- **WHEN** downsampling is performed
- **THEN** Gaussian-weighted bilinear sampling SHALL be used
- **AND** 4 bilinear taps SHALL approximate a Gaussian kernel
- **AND** effective sampling SHALL cover 16 source texels

#### Scenario: Filter type exposed as parameter

- **GIVEN** a DownsamplePass instance
- **WHEN** `get_shader_parameters()` is called
- **THEN** a parameter named "filter_type" SHALL be returned
- **AND** min SHALL be 0, max SHALL be 1, step SHALL be 1

#### Scenario: Filter type runtime change

- **GIVEN** DownsamplePass is actively rendering
- **WHEN** `set_shader_parameter("filter_type", 1.0)` is called
- **THEN** the next frame SHALL use Gaussian filter
- **AND** no pipeline rebuild SHALL occur

### Requirement: Unified Pass Parameter UI

The ImGui layer SHALL provide a reusable helper for rendering pass parameter controls.

#### Scenario: Parameter sliders rendered for pass with parameters

- **GIVEN** a Pass with non-empty `get_shader_parameters()` result
- **WHEN** the parameter UI helper is invoked
- **THEN** a slider SHALL be rendered for each parameter
- **AND** slider range SHALL use min/max from parameter metadata
- **AND** slider step SHALL use step from parameter metadata

#### Scenario: Enum-style parameter rendered as combo box

- **GIVEN** a parameter with step = 1 and integer min/max range
- **WHEN** the parameter UI helper is invoked
- **THEN** a combo box MAY be rendered instead of a slider
- **AND** values SHALL map to descriptive labels

#### Scenario: No UI rendered for pass without parameters

- **GIVEN** a Pass with empty `get_shader_parameters()` result
- **WHEN** the parameter UI helper is invoked
- **THEN** no UI elements SHALL be rendered

#### Scenario: Parameter changes propagate to pass

- **GIVEN** a parameter control is displayed
- **WHEN** the user adjusts the value
- **THEN** `set_shader_parameter(name, value)` SHALL be called on the pass
- **AND** the change SHALL be reflected in the next rendered frame
