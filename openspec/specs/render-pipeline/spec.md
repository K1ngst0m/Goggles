# render-pipeline Specification

## Purpose
TBD - created by archiving change add-render-pipeline. Update Purpose after archive.
## Requirements
### Requirement: Shader Runtime Compilation

The render shader subsystem SHALL compile Slang shaders to SPIR-V at runtime using the Slang API, supporting both HLSL-style native shaders and GLSL-style RetroArch shaders.

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

#### Scenario: GLSL mode for RetroArch shaders

- **GIVEN** a RetroArch `.slang` file using GLSL syntax (`#version 450`, `vec4`, `sampler2D`)
- **WHEN** `compile_retroarch_shader()` is called
- **THEN** `ShaderRuntime` SHALL use a GLSL-enabled Slang session
- **AND** the session SHALL have `enableGLSL = true` and `allowGLSLSyntax = true`

#### Scenario: Dual session architecture

- **GIVEN** `ShaderRuntime` is initialized
- **THEN** it SHALL maintain two Slang sessions: one for HLSL-native shaders, one for GLSL-RetroArch shaders
- **AND** both sessions SHALL share the same global session
- **AND** both sessions SHALL target SPIR-V 1.3

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
- **WHEN** `OutputPass` is initialized
- **THEN** pipeline and descriptor layout SHALL be created
- **AND** pipeline SHALL be created with `VkPipelineRenderingCreateInfo` specifying target format
- **AND** all Vulkan resources SHALL use RAII (`vk::Unique*`)

#### Scenario: Frame rendering

- **GIVEN** an imported texture and acquired swapchain image
- **WHEN** `OutputPass` records commands
- **THEN** dynamic rendering SHALL be begun with the swapchain image view
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

- **GIVEN** the render module structure
- **THEN** code SHALL be organized into `backend/`, `shader/`, and `chain/` directories
- **AND** Vulkan backend SHALL be isolated from shader processing

#### Scenario: Error handling macros

- **GIVEN** Vulkan API calls that return `vk::Result`
- **WHEN** error checking is needed
- **THEN** `VK_TRY(call, code, msg)` macro SHALL be used for early return
- **AND** error message SHALL include the Vulkan result string

#### Scenario: Result propagation

- **GIVEN** internal functions that return `Result<T>`
- **WHEN** the result needs to be propagated to the caller
- **THEN** `GOGGLES_TRY(expr)` macro SHALL be used for early return

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

### Requirement: OutputPass Behavior

The `OutputPass` SHALL serve as combined normalize+output pass until multi-pass chains are implemented.

#### Scenario: Direct DMA-BUF to swapchain

- **GIVEN** no RetroArch shader passes are configured
- **WHEN** OutputPass processes a frame
- **THEN** it SHALL sample `ctx.source_texture` (the DMA-BUF import)
- **AND** begin dynamic rendering with `ctx.target_image_view`
- **AND** use `ctx.frame_index` for descriptor set selection

#### Scenario: Backend provides image views

- **GIVEN** swapchain is recreated (resize)
- **WHEN** backend recreates swapchain image views
- **THEN** OutputPass SHALL NOT need reinitialization
- **AND** new image views SHALL be passed via `PassContext.target_image_view`

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

### Requirement: Vulkan Validation Layer Support

The render backend SHALL support optional Vulkan validation layer integration for development-time error detection.

#### Scenario: Validation enabled via config

- **GIVEN** `goggles.toml` has `[render] enable_validation = true`
- **WHEN** `VulkanBackend::init()` is called
- **THEN** `VK_LAYER_KHRONOS_validation` SHALL be enabled if available
- **AND** `VK_EXT_debug_utils` extension SHALL be enabled
- **AND** a debug messenger SHALL be created to capture validation messages

#### Scenario: Validation disabled via config

- **GIVEN** `goggles.toml` has `[render] enable_validation = false` or field is absent
- **WHEN** `VulkanBackend::init()` is called
- **THEN** no validation layers SHALL be enabled
- **AND** no debug messenger SHALL be created

#### Scenario: Validation layer unavailable

- **GIVEN** `VK_LAYER_KHRONOS_validation` is not installed on the system
- **WHEN** validation is requested via config
- **THEN** a warning SHALL be logged via `GOGGLES_LOG_WARN`
- **AND** instance creation SHALL proceed without validation
- **AND** no error SHALL be returned

#### Scenario: Validation message routing

- **GIVEN** validation layer is enabled and debug messenger is active
- **WHEN** a validation message is generated
- **THEN** ERROR severity messages SHALL be logged via `GOGGLES_LOG_ERROR`
- **AND** WARNING severity messages SHALL be logged via `GOGGLES_LOG_WARN`
- **AND** INFO severity messages SHALL be logged via `GOGGLES_LOG_DEBUG`
- **AND** VERBOSE severity messages SHALL be logged via `GOGGLES_LOG_TRACE`

### Requirement: Validation Configuration Setting

The application config SHALL include a setting to control Vulkan validation layer enablement.

#### Scenario: Config field definition

- **GIVEN** the `goggles::Config` struct
- **WHEN** `Config::Render` is defined
- **THEN** it SHALL include `bool enable_validation` field
- **AND** the default value SHALL be `false`

#### Scenario: TOML parsing

- **GIVEN** `goggles.toml` contains `[render] enable_validation = true`
- **WHEN** `load_config()` is called
- **THEN** `config.render.enable_validation` SHALL be `true`

#### Scenario: Missing config field

- **GIVEN** `goggles.toml` does not contain `enable_validation` field
- **WHEN** `load_config()` is called
- **THEN** `config.render.enable_validation` SHALL default to `false`

### Requirement: Debug Messenger RAII

The debug messenger resource SHALL be managed via RAII wrapper class.

#### Scenario: Messenger creation

- **GIVEN** validation is enabled and instance is created
- **WHEN** `VulkanDebugMessenger::create(instance)` is called
- **THEN** a `Result<VulkanDebugMessenger>` SHALL be returned
- **AND** on success, the messenger SHALL be active and routing messages
- **AND** on failure, an appropriate `Error` SHALL be returned

#### Scenario: Messenger destruction order

- **GIVEN** `VulkanBackend` owns both instance and debug messenger
- **WHEN** `VulkanBackend` is destroyed
- **THEN** debug messenger SHALL be destroyed before instance
- **AND** no use-after-free SHALL occur

### Requirement: Dynamic Rendering

The render backend SHALL use Vulkan 1.3 dynamic rendering instead of traditional render passes for all rendering operations.

#### Scenario: API version requirement

- **GIVEN** `VulkanBackend::create_instance()` is called
- **WHEN** the Vulkan instance is created
- **THEN** `VkApplicationInfo.apiVersion` SHALL be `VK_API_VERSION_1_3`

#### Scenario: Dynamic rendering feature enablement

- **GIVEN** `VulkanBackend::create_device()` is called
- **WHEN** the logical device is created
- **THEN** `VkPhysicalDeviceDynamicRenderingFeatures.dynamicRendering` SHALL be enabled
- **AND** the feature SHALL be verified as supported before device creation

#### Scenario: No VkRenderPass or VkFramebuffer objects

- **GIVEN** the render backend is initialized
- **THEN** no `VkRenderPass` objects SHALL be created
- **AND** no `VkFramebuffer` objects SHALL be created
- **AND** pipelines SHALL be created with `VkPipelineRenderingCreateInfo` instead of `renderPass`

#### Scenario: Command buffer rendering

- **GIVEN** a pass records rendering commands
- **WHEN** rendering to a target image
- **THEN** `vkCmdBeginRendering()` SHALL be used instead of `vkCmdBeginRenderPass()`
- **AND** `vkCmdEndRendering()` SHALL be used instead of `vkCmdEndRenderPass()`
- **AND** `VkRenderingInfo` SHALL specify the target image view and format directly

### Requirement: RetroArch Shader Preprocessing

The shader subsystem SHALL preprocess RetroArch `.slang` files before compilation to handle custom pragmas.

#### Scenario: Include resolution

- **GIVEN** a `.slang` file with `#include "path/to/file.inc"`
- **WHEN** preprocessing is performed
- **THEN** the include directive SHALL be replaced with the file contents
- **AND** paths SHALL be resolved relative to the including file
- **AND** nested includes SHALL be resolved recursively

#### Scenario: Stage splitting

- **GIVEN** a `.slang` file with `#pragma stage vertex` and `#pragma stage fragment`
- **WHEN** preprocessing is performed
- **THEN** source SHALL be split into separate vertex and fragment sources
- **AND** shared declarations before first pragma SHALL be included in both stages
- **AND** pragmas SHALL be removed from output source

#### Scenario: Parameter extraction

- **GIVEN** a `.slang` file with `#pragma parameter NAME "Description" default min max step`
- **WHEN** preprocessing is performed
- **THEN** parameter metadata SHALL be extracted (name, description, default, min, max, step)
- **AND** pragma lines SHALL be removed from output source
- **AND** parameters SHALL be available for semantic binding

#### Scenario: Metadata extraction

- **GIVEN** a `.slang` file with `#pragma name ALIAS` or `#pragma format FORMAT`
- **WHEN** preprocessing is performed
- **THEN** name alias and format SHALL be extracted as pass metadata
- **AND** pragma lines SHALL be removed from output source

### Requirement: Preset Parser

The shader subsystem SHALL parse RetroArch `.slangp` preset files to configure multi-pass shader chains.

#### Scenario: Preset file loading

- **GIVEN** a `.slangp` preset file with `shaders = N` and per-pass configuration
- **WHEN** `PresetParser::load()` is called
- **THEN** a `PresetConfig` struct SHALL be returned
- **AND** it SHALL contain shader paths, scale types, filter modes, and format overrides for each pass

#### Scenario: Scale type parsing

- **GIVEN** a preset with `scale_type0 = source` or `scale_type0 = viewport` or `scale_type0 = absolute`
- **WHEN** preset is parsed
- **THEN** scale type SHALL be stored per pass
- **AND** `scale0` or `scale0_x`/`scale0_y` SHALL be parsed as scale factors

#### Scenario: Filter mode parsing

- **GIVEN** a preset with `filter_linear0 = true` or `filter_linear0 = false`
- **WHEN** preset is parsed
- **THEN** sampler filter mode SHALL be stored per pass (linear or nearest)

#### Scenario: Framebuffer format parsing

- **GIVEN** a preset with `float_framebuffer0 = true` or `srgb_framebuffer0 = true`
- **WHEN** preset is parsed
- **THEN** framebuffer format SHALL be stored (R16G16B16A16_SFLOAT, R8G8B8A8_SRGB, or R8G8B8A8_UNORM default)

### Requirement: FilterPass Implementation

The chain subsystem SHALL provide `FilterPass` for executing RetroArch shader passes.

#### Scenario: FilterPass initialization

- **GIVEN** a preprocessed RetroArch shader (vertex SPIR-V, fragment SPIR-V, parameters, metadata)
- **WHEN** `FilterPass::init()` is called
- **THEN** graphics pipeline SHALL be created with both shader stages
- **AND** descriptor layout SHALL match reflected bindings (UBO, textures)
- **AND** `num_sync_indices` descriptor sets SHALL be allocated

#### Scenario: Framebuffer creation for intermediate passes

- **GIVEN** a FilterPass that is NOT the final pass
- **WHEN** initialized with scale type and output dimensions
- **THEN** a framebuffer image SHALL be created with specified format
- **AND** image view SHALL be created for sampling by subsequent passes

#### Scenario: FilterPass recording

- **GIVEN** a FilterPass with valid pipeline and framebuffer
- **WHEN** `record()` is called with PassContext
- **THEN** dynamic rendering SHALL begin with framebuffer image view (or `ctx.target_image_view` for final pass)
- **AND** UBO SHALL be updated with semantic values (MVP, sizes)
- **AND** push constants SHALL be updated with per-frame values (FrameCount, sizes)
- **AND** descriptor set SHALL bind Source and Original textures

### Requirement: Slang Native Reflection

The shader subsystem SHALL use Slang's built-in reflection API to discover shader bindings without external dependencies.

#### Scenario: Reflection from linked program

- **GIVEN** a linked `IComponentType` from Slang compilation
- **WHEN** `getLayout()` is called
- **THEN** a `ProgramLayout*` SHALL be returned
- **AND** it SHALL provide access to all shader parameters

#### Scenario: Push constant discovery

- **GIVEN** a shader with push constants containing `SourceSize`, `OutputSize`, `FrameCount`, user params
- **WHEN** reflection iterates parameters with `SLANG_PARAMETER_CATEGORY_PUSH_CONSTANT_BUFFER`
- **THEN** each member's byte offset SHALL be discoverable via `getOffset()`
- **AND** member names SHALL match shader source declarations

#### Scenario: UBO member discovery

- **GIVEN** a shader with UBO containing `MVP` matrix
- **WHEN** reflection iterates the UBO type layout
- **THEN** `MVP` offset SHALL be discoverable via `getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM)`
- **AND** UBO binding/set SHALL be discoverable via `getBindingIndex()` / `getBindingSpace()`

#### Scenario: Texture binding discovery

- **GIVEN** a shader with `Source` sampler at set=0, binding=2
- **WHEN** reflection iterates texture parameters
- **THEN** binding index SHALL be 2
- **AND** binding space (set) SHALL be 0
- **AND** parameter name SHALL be "Source"

### Requirement: Semantic Binder

The chain subsystem SHALL populate shader uniforms with RetroArch semantic values based on reflection data.

#### Scenario: Size semantic population

- **GIVEN** a shader with `SourceSize`, `OutputSize`, `OriginalSize` in push constants
- **WHEN** semantic binder populates values before draw
- **THEN** each size SHALL be written as vec4 `[width, height, 1/width, 1/height]`
- **AND** values SHALL reflect actual texture/output dimensions

#### Scenario: Frame counter population

- **GIVEN** a shader with `FrameCount` in push constants
- **WHEN** semantic binder populates values
- **THEN** `FrameCount` SHALL be set to current frame number (monotonically increasing)

#### Scenario: MVP matrix population

- **GIVEN** a shader with `MVP` in UBO
- **WHEN** semantic binder populates UBO
- **THEN** `MVP` SHALL be set to identity matrix (or orthographic projection for proper UV mapping)

#### Scenario: Texture binding

- **GIVEN** a FilterPass with Source and Original texture semantics
- **WHEN** descriptor set is updated before draw
- **THEN** `Source` SHALL be bound to previous pass output (or Original for pass 0)
- **AND** `Original` SHALL be bound to the normalized captured frame

### Requirement: zfast-crt Verification

The RetroArch shader support SHALL be verified using the zfast-crt shader as minimal test case.

#### Scenario: zfast-crt compilation

- **GIVEN** the zfast-crt `.slang` file from `research/slang-shaders/crt/shaders/zfast_crt/`
- **WHEN** loaded via ShaderRuntime
- **THEN** preprocessing SHALL extract parameters (BLURSCALE, LOWLUMSCAN, etc.)
- **AND** compilation SHALL succeed without errors
- **AND** SPIR-V reflection SHALL identify expected bindings

#### Scenario: zfast-crt rendering

- **GIVEN** a compiled zfast-crt shader in a FilterPass
- **WHEN** rendered with a test input texture
- **THEN** output SHALL exhibit CRT-style scanlines and blur
- **AND** visual output SHALL match RetroArch reference (manual verification)

