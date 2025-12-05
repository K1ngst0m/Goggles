# render-pipeline Specification

## Purpose
TBD - created by archiving change add-render-pipeline. Update Purpose after archive.
## Requirements
### Requirement: Shader Runtime Compilation

The render shader subsystem SHALL compile Slang shaders to SPIR-V at runtime using the Slang API.

#### Scenario: First-run compilation

- **GIVEN** a `.slang` shader file exists in `shaders/`
- **WHEN** the shader is requested and no cached SPIR-V exists
- **THEN** `ShaderRuntime` SHALL compile the shader via Slang
- **AND** cache the resulting SPIR-V to disk
- **AND** return the SPIR-V bytecode

#### Scenario: Cache hit

- **GIVEN** a cached `.spv` file exists with matching source hash
- **WHEN** the shader is requested
- **THEN** `ShaderRuntime` SHALL load SPIR-V from cache
- **AND** skip compilation

#### Scenario: Cache invalidation

- **GIVEN** a cached `.spv` file exists
- **WHEN** the source `.slang` file has changed
- **THEN** `ShaderRuntime` SHALL recompile the shader
- **AND** update the cache

### Requirement: Fullscreen Blit Pipeline

The render backend SHALL provide a graphics pipeline for blitting imported textures to the swapchain with exact pixel value preservation.

#### Scenario: Passthrough rendering

- **GIVEN** an imported DMA-BUF captured from source app's swapchain
- **AND** swapchain format matches source color space (per Swapchain Format Matching)
- **WHEN** rendered to Goggles swapchain
- **THEN** the output SHALL match the original application's visual appearance exactly
- **AND** the fragment shader SHALL only sample and output (no color math)

#### Scenario: Pipeline initialization

- **GIVEN** valid SPIR-V bytecode from `ShaderRuntime`
- **WHEN** `BlitPipeline` is initialized
- **THEN** render pass, pipeline, descriptor layout, and sampler SHALL be created
- **AND** all Vulkan resources SHALL use RAII (`vk::Unique*`)

#### Scenario: Frame rendering

- **GIVEN** an imported texture and acquired swapchain image
- **WHEN** `BlitPipeline` records commands
- **THEN** a render pass SHALL be begun with the swapchain framebuffer
- **AND** the imported texture SHALL be bound via descriptor set
- **AND** a fullscreen triangle SHALL be drawn

### Requirement: Texture Sampling

The blit pipeline SHALL sample imported textures using a Vulkan sampler with linear filtering.

#### Scenario: Sampler configuration

- **GIVEN** `BlitPipeline` is initialized
- **WHEN** the sampler is created
- **THEN** filter mode SHALL be `VK_FILTER_LINEAR`
- **AND** address mode SHALL be `VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE`

### Requirement: Swapchain Format Matching

The render backend SHALL match swapchain format color space to source image format to ensure correct pixel value preservation.

#### Scenario: UNORM source format

- **GIVEN** captured image uses a UNORM format (e.g., `A2R10G10B10_UNORM`)
- **WHEN** swapchain is created or format mismatch detected
- **THEN** swapchain SHALL use `B8G8R8A8_UNORM` format
- **AND** sampler SHALL NOT linearize values
- **AND** hardware SHALL NOT apply gamma encoding on write

#### Scenario: SRGB source format

- **GIVEN** captured image uses an SRGB format (e.g., `B8G8R8A8_SRGB`)
- **WHEN** swapchain is created or format mismatch detected
- **THEN** swapchain SHALL use `B8G8R8A8_SRGB` format
- **AND** sampler linearization and hardware encoding SHALL cancel out

#### Scenario: Format change triggers swapchain recreation

- **GIVEN** swapchain exists with one color space type
- **WHEN** source format changes to different color space type
- **THEN** swapchain SHALL be recreated with matching format

### Requirement: Pipeline Extensibility

The render architecture SHALL support future multi-pass RetroArch shader processing through a modular structure.

#### Scenario: Module organization

- **GIVEN** the render subsystem
- **WHEN** organized into modules
- **THEN** `render/backend/` SHALL contain Vulkan API abstraction
- **AND** `render/shader/` SHALL contain shader compilation infrastructure
- **AND** `render/chain/` SHALL contain filter chain and pass implementations

#### Scenario: Future pipeline model

- **GIVEN** RetroArch shader support is added
- **WHEN** pipeline processes a frame
- **THEN** input normalization SHALL occur first (to RGBA16F linear working format)
- **AND** shader passes SHALL process in working format
- **AND** output conversion SHALL occur last (to swapchain format)

#### Scenario: Current minimal implementation

- **GIVEN** no RetroArch shaders are configured
- **WHEN** pipeline processes a frame
- **THEN** single blit pass SHALL handle both input and output roles
- **AND** this pass SHALL become the output stage when multi-pass is added

### Requirement: Pass Infrastructure

The render chain subsystem SHALL provide a Pass abstraction compatible with RetroArch shader system.

#### Scenario: Pass interface

- **GIVEN** a Pass implementation
- **WHEN** initialized with device, render pass, num_sync_indices, and shader runtime
- **THEN** the pass SHALL create its pipeline and per-frame descriptor sets
- **AND** allocate `num_sync_indices` descriptor sets from its pool

#### Scenario: PassContext provides textures

- **GIVEN** a PassContext for recording
- **WHEN** passed to `Pass::record()`
- **THEN** it SHALL contain `source_texture` (previous pass output)
- **AND** it SHALL contain `original_texture` (normalized input)
- **AND** it SHALL contain `frame_index` for descriptor set selection
- **AND** it SHALL contain `target_framebuffer` for rendering

#### Scenario: Per-frame descriptor isolation

- **GIVEN** num_sync_indices = 2
- **WHEN** frame N is recording while frame N-1 is still on GPU
- **THEN** the pass SHALL update `descriptor_sets[N % 2]`
- **AND** NOT touch `descriptor_sets[(N-1) % 2]`
- **AND** no validation error SHALL occur

### Requirement: OutputPass Behavior

The `OutputPass` SHALL serve as combined normalize+output pass until multi-pass chains are implemented.

#### Scenario: Direct DMA-BUF to swapchain

- **GIVEN** no RetroArch shader passes are configured
- **WHEN** OutputPass processes a frame
- **THEN** it SHALL sample `ctx.source_texture` (the DMA-BUF import)
- **AND** render to `ctx.target_framebuffer` (swapchain)
- **AND** use `ctx.frame_index` for descriptor set selection

#### Scenario: Backend provides framebuffers

- **GIVEN** swapchain is recreated (resize)
- **WHEN** backend recreates framebuffers
- **THEN** OutputPass SHALL NOT need reinitialization
- **AND** new framebuffers SHALL be passed via PassContext

### Requirement: Future RetroArch Integration

The Pass abstraction SHALL support future RetroArch shader passes.

#### Scenario: Multi-pass chain (Phase 2+)

- **GIVEN** RetroArch shader passes are configured
- **WHEN** filter chain processes a frame
- **THEN** NormalizePass (first) SHALL output "Original" texture
- **AND** RetroArch passes SHALL receive Source + Original via PassContext
- **AND** OutputPass (last) SHALL convert to swapchain format

#### Scenario: PassContext extensibility

- **GIVEN** RetroArch semantics require OriginalHistory, PassFeedback, etc.
- **WHEN** PassContext is extended
- **THEN** existing passes SHALL NOT require modification
- **AND** new texture bindings SHALL be added to PassContext struct

