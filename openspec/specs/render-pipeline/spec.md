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

The render backend SHALL provide a graphics pipeline for blitting imported textures to the swapchain, initialized via typed config structs.

#### Scenario: Pipeline initialization

- **GIVEN** valid SPIR-V bytecode from `ShaderRuntime`
- **WHEN** `OutputPass` is initialized via `init(const VulkanContext&, ShaderRuntime&, const OutputPassConfig&)`
- **THEN** pipeline and descriptor layout SHALL be created
- **AND** pipeline SHALL be created with `VkPipelineRenderingCreateInfo` specifying target format from config
- **AND** all Vulkan resources SHALL use RAII (`vk::Unique*`)

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

The `OutputPass` SHALL serve as the final post-chain pass, rendering to the swapchain.

#### Scenario: OutputPass in post-chain vector

- **GIVEN** `FilterChain` is initialized
- **THEN** `OutputPass` SHALL be stored in `m_postchain_passes` vector
- **AND** there SHALL NOT be a separate `m_output_pass` member pointer
- **AND** `m_postchain_passes.back()` SHALL reference the OutputPass

#### Scenario: Direct DMA-BUF to swapchain (no RetroArch passes)

- **GIVEN** no RetroArch shader passes are configured
- **WHEN** OutputPass processes a frame
- **THEN** it SHALL sample `ctx.source_texture` (the pre-chain output or DMA-BUF import)
- **AND** begin dynamic rendering with `ctx.target_image_view`
- **AND** use `ctx.frame_index` for descriptor set selection

#### Scenario: Post-RetroArch to swapchain

- **GIVEN** RetroArch shader passes are configured
- **WHEN** OutputPass (as final post-chain pass) processes a frame
- **THEN** it SHALL sample the previous post-chain pass output (or RetroArch output if first)
- **AND** render to the swapchain image view

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

FilterPass SHALL create Vulkan pipeline resources based on shader reflection data from SlangReflect.

The pipeline creation process SHALL:
1. Query push constant size from reflection and use it for VkPushConstantRange
2. Build descriptor set layout from reflected UBO and texture bindings
3. Combine stage flags when a binding is used by both vertex and fragment stages
4. Create vertex input state matching reflected vertex shader inputs
5. Allocate descriptor pool with correct type counts for all binding types

#### Scenario: Shader with UBO and textures
- **WHEN** a shader has UBO at binding 0 (vertex+fragment) and texture at binding 2
- **THEN** descriptor layout includes both bindings with correct stage flags
- **AND** descriptor pool has capacity for uniform buffer and combined image sampler

#### Scenario: Shader with extended push constants
- **WHEN** a shader's push constant block is 76 bytes
- **THEN** VkPushConstantRange.size is set to 76
- **AND** pipeline creation succeeds without validation errors

#### Scenario: Shader with vertex inputs
- **WHEN** a shader expects Position (location 0, vec4) and TexCoord (location 1, vec2)
- **THEN** vertex input binding description specifies correct stride
- **AND** vertex attribute descriptions specify locations 0 and 1 with R32G32B32A32_SFLOAT and R32G32_SFLOAT formats

### Requirement: Slang Native Reflection

SlangReflect SHALL expose additional reflection data for pipeline creation:

1. `push_constant_size()` - total size in bytes of push constant block
2. `get_ubo_bindings()` - list of UBO bindings with set, binding, size, and stage flags
3. `get_vertex_inputs()` - list of vertex inputs with location, format, and offset

#### Scenario: Reflect push constant size
- **WHEN** shader has a push constant block with 5 vec4 members
- **THEN** push_constant_size() returns 80

#### Scenario: Reflect UBO with combined stages
- **WHEN** vertex and fragment shaders both reference UBO at binding 0
- **THEN** get_ubo_bindings() returns entry with stage_flags = VERTEX | FRAGMENT

#### Scenario: Reflect vertex inputs
- **WHEN** vertex shader has `layout(location = 0) in vec4 Position`
- **THEN** get_vertex_inputs() includes {location: 0, format: R32G32B32A32_SFLOAT}

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

### Requirement: Aspect Ratio Display Modes

The output pass SHALL support four display modes for scaling captured frames to the output window, controlled by configuration.

#### Scenario: Fit mode scales image to fit within window

- **GIVEN** scale mode is set to `fit`
- **AND** source image has aspect ratio different from window
- **WHEN** `OutputPass::record()` renders the frame
- **THEN** the viewport SHALL be calculated to show the entire image
- **AND** the image SHALL be centered in the window
- **AND** letterbox (horizontal bars) or pillarbox (vertical bars) SHALL fill unused areas with black

#### Scenario: Fill mode scales image to cover entire window

- **GIVEN** scale mode is set to `fill`
- **AND** source image has aspect ratio different from window
- **WHEN** `OutputPass::record()` renders the frame
- **THEN** the viewport SHALL be calculated to cover the entire window
- **AND** the image SHALL be centered
- **AND** portions of the image extending beyond window bounds SHALL be clipped by scissor

#### Scenario: Stretch mode matches window dimensions exactly

- **GIVEN** scale mode is set to `stretch`
- **WHEN** `OutputPass::record()` renders the frame
- **THEN** the viewport SHALL cover the entire window
- **AND** the image SHALL be scaled to match window dimensions exactly
- **AND** aspect ratio distortion is acceptable

#### Scenario: Integer mode with auto scale finds maximum fit

- **GIVEN** scale mode is set to `integer`
- **AND** integer_scale is `0` (auto)
- **AND** source is 640x480 and window is 1920x1080
- **WHEN** `OutputPass::record()` renders the frame
- **THEN** the maximum integer scale that fits SHALL be calculated (2x = 1280x960)
- **AND** the image SHALL be centered with black borders

#### Scenario: Integer mode with fixed scale of 1 shows original size

- **GIVEN** scale mode is set to `integer`
- **AND** integer_scale is `1`
- **AND** source is 640x480 and window is 1920x1080
- **WHEN** `OutputPass::record()` renders the frame
- **THEN** the viewport SHALL be 640x480 (original size)
- **AND** the image SHALL be centered with black borders

#### Scenario: Integer mode with fixed scale multiplies source dimensions

- **GIVEN** scale mode is set to `integer`
- **AND** integer_scale is `3`
- **AND** source is 640x480
- **WHEN** `OutputPass::record()` renders the frame
- **THEN** the viewport SHALL be 1920x1440 (3x source)
- **AND** portions exceeding window bounds SHALL be clipped by scissor

#### Scenario: Same aspect ratio produces identical output for fit/fill/stretch

- **GIVEN** source image and window have the same aspect ratio
- **WHEN** fit, fill, or stretch mode is used
- **THEN** the output SHALL be identical regardless of mode
- **AND** the image SHALL fill the entire window

### Requirement: Scale Mode Configuration

The application config SHALL include a setting to control the display scale mode.

#### Scenario: Config field definition

- **GIVEN** the `goggles::Config` struct
- **WHEN** `Config::Render` is defined
- **THEN** it SHALL include `ScaleMode scale_mode` field
- **AND** the default value SHALL be `ScaleMode::Stretch`

#### Scenario: TOML parsing for fit mode

- **GIVEN** `goggles.toml` contains `[render] scale_mode = "fit"`
- **WHEN** `load_config()` is called
- **THEN** `config.render.scale_mode` SHALL be `ScaleMode::Fit`

#### Scenario: TOML parsing for fill mode

- **GIVEN** `goggles.toml` contains `[render] scale_mode = "fill"`
- **WHEN** `load_config()` is called
- **THEN** `config.render.scale_mode` SHALL be `ScaleMode::Fill`

#### Scenario: TOML parsing for stretch mode

- **GIVEN** `goggles.toml` contains `[render] scale_mode = "stretch"`
- **WHEN** `load_config()` is called
- **THEN** `config.render.scale_mode` SHALL be `ScaleMode::Stretch`

#### Scenario: TOML parsing for integer mode

- **GIVEN** `goggles.toml` contains `[render] scale_mode = "integer"`
- **WHEN** `load_config()` is called
- **THEN** `config.render.scale_mode` SHALL be `ScaleMode::Integer`

#### Scenario: Missing config field uses default

- **GIVEN** `goggles.toml` does not contain `scale_mode` field
- **WHEN** `load_config()` is called
- **THEN** `config.render.scale_mode` SHALL default to `ScaleMode::Stretch`

#### Scenario: Invalid config value produces error

- **GIVEN** `goggles.toml` contains `[render] scale_mode = "invalid_value"`
- **WHEN** `load_config()` is called
- **THEN** an error SHALL be returned
- **AND** the error message SHALL indicate the invalid value

### Requirement: Integer Scale Configuration

The application config SHALL include a setting to control the integer scaling multiplier when scale_mode is "integer".

#### Scenario: Integer scale field definition

- **GIVEN** the `goggles::Config` struct
- **WHEN** `Config::Render` is defined
- **THEN** it SHALL include `uint32_t integer_scale` field
- **AND** the default value SHALL be `0` (auto)

#### Scenario: Integer scale only applies in integer mode

- **GIVEN** `goggles.toml` contains `[render] scale_mode = "stretch"` and `integer_scale = 2`
- **WHEN** rendering occurs
- **THEN** the `integer_scale` value SHALL be ignored
- **AND** stretch mode behavior SHALL apply

#### Scenario: TOML parsing for auto integer scale

- **GIVEN** `goggles.toml` contains `[render] integer_scale = 0`
- **WHEN** `load_config()` is called
- **THEN** `config.render.integer_scale` SHALL be `0`

#### Scenario: TOML parsing for fixed integer scale

- **GIVEN** `goggles.toml` contains `[render] integer_scale = 3`
- **WHEN** `load_config()` is called
- **THEN** `config.render.integer_scale` SHALL be `3`

#### Scenario: Integer scale validation

- **GIVEN** `goggles.toml` contains `[render] integer_scale = 10`
- **WHEN** `load_config()` is called
- **THEN** an error SHALL be returned
- **AND** the error message SHALL indicate valid range is 0-8

### Requirement: FinalViewportSize Calculation

The filter chain SHALL calculate `FinalViewportSize` based on the scale mode to ensure correct shader behavior when `scale_type = viewport` is used.

#### Scenario: Stretch mode uses swapchain size

- **GIVEN** scale mode is `stretch`
- **AND** swapchain size is 1920x1080
- **WHEN** `FinalViewportSize` is calculated
- **THEN** it SHALL be (1920, 1080)

#### Scenario: Fit mode uses letterboxed effective area

- **GIVEN** scale mode is `fit`
- **AND** swapchain size is 1920x1080 (16:9)
- **AND** source aspect ratio is 4:3
- **WHEN** `FinalViewportSize` is calculated
- **THEN** it SHALL be (1440, 1080) representing the effective content area
- **AND** shaders using `scale_type = viewport` SHALL render at this resolution

#### Scenario: Fill mode uses scaled area exceeding bounds

- **GIVEN** scale mode is `fill`
- **AND** swapchain size is 1920x1080 (16:9)
- **AND** source aspect ratio is 4:3
- **WHEN** `FinalViewportSize` is calculated
- **THEN** it SHALL be (1920, 1440) representing the full scaled content
- **AND** the OutputPass scissor SHALL clip to swapchain bounds

#### Scenario: Integer mode uses source multiplied by scale factor

- **GIVEN** scale mode is `integer`
- **AND** integer_scale is `2`
- **AND** source is 640x480
- **WHEN** `FinalViewportSize` is calculated
- **THEN** it SHALL be (1280, 960)
- **AND** shaders using `scale_type = viewport` SHALL render at this resolution

#### Scenario: Integer mode auto calculates max scale

- **GIVEN** scale mode is `integer`
- **AND** integer_scale is `0` (auto)
- **AND** source is 640x480 and swapchain is 1920x1080
- **WHEN** `FinalViewportSize` is calculated
- **THEN** max scale SHALL be min(floor(1920/640), floor(1080/480)) = min(3, 2) = 2
- **AND** FinalViewportSize SHALL be (1280, 960)

#### Scenario: SemanticBinder uses calculated FinalViewportSize

- **GIVEN** a shader pass with `FinalViewportSize` semantic
- **WHEN** SemanticBinder populates push constants
- **THEN** `FinalViewportSize` SHALL reflect the calculated value based on scale mode
- **AND** NOT the raw swapchain dimensions (except in stretch mode)

### Requirement: Viewport Calculation Utility

The render subsystem SHALL provide a utility function to calculate scaled viewport parameters.

#### Scenario: Calculate fit viewport

- **GIVEN** source extent (640, 480) and target extent (1920, 1080)
- **WHEN** `calculate_viewport()` is called with `ScaleMode::Fit`
- **THEN** result SHALL have width=1440, height=1080
- **AND** offset_x=240, offset_y=0 (centered horizontally)

#### Scenario: Calculate fill viewport

- **GIVEN** source extent (640, 480) and target extent (1920, 1080)
- **WHEN** `calculate_viewport()` is called with `ScaleMode::Fill`
- **THEN** result SHALL have width=1920, height=1440
- **AND** offset_x=0, offset_y=-180 (centered, extends beyond bounds)

#### Scenario: Calculate stretch viewport

- **GIVEN** source extent (640, 480) and target extent (1920, 1080)
- **WHEN** `calculate_viewport()` is called with `ScaleMode::Stretch`
- **THEN** result SHALL have width=1920, height=1080
- **AND** offset_x=0, offset_y=0

#### Scenario: Calculate integer viewport with auto scale

- **GIVEN** source extent (640, 480) and target extent (1920, 1080)
- **WHEN** `calculate_viewport()` is called with `ScaleMode::Integer` and integer_scale=0
- **THEN** result SHALL have width=1280, height=960 (2x)
- **AND** offset_x=320, offset_y=60 (centered)

#### Scenario: Calculate integer viewport with fixed scale

- **GIVEN** source extent (640, 480) and target extent (1920, 1080)
- **WHEN** `calculate_viewport()` is called with `ScaleMode::Integer` and integer_scale=1
- **THEN** result SHALL have width=640, height=480
- **AND** offset_x=640, offset_y=300 (centered)

### Requirement: PassContext Source Extent

The PassContext struct SHALL include source image dimensions to enable aspect ratio calculations.

#### Scenario: Source extent available for aspect ratio calculation

- **GIVEN** a captured frame with known dimensions
- **WHEN** PassContext is created for OutputPass
- **THEN** `source_extent` SHALL contain the width and height of the source image
- **AND** the values SHALL be used for aspect ratio mode calculations

### Requirement: Preset Texture Assets

The filter chain SHALL load external textures listed in a RetroArch preset `textures` entry and bind them by name to matching sampler uniforms.

#### Scenario: Mask LUTs loaded and bound
- **WHEN** a preset defining `textures = "mask_a;mask_b"` with paths for each name is loaded
- **THEN** each texture SHALL be decoded and uploaded to a GPU image
- **AND** each texture SHALL be bound to the sampler with the same name in the shader

### Requirement: Preset Texture Sampling Overrides

The filter chain SHALL honor per-texture sampling flags from the preset (`*_linear`, `*_mipmap`, `*_wrap_mode`).

#### Scenario: Repeat + mipmapped mask texture
- **GIVEN** a preset sets `mask_grille_texture_large_wrap_mode = "repeat"` and `mask_grille_texture_large_mipmap = true`
- **WHEN** the preset is loaded
- **THEN** the bound sampler SHALL use repeat addressing and mipmapped sampling

### Requirement: Alias Pass Routing

The filter chain SHALL expose aliased pass outputs as named textures for subsequent passes, and SHALL provide `ALIASSize` push constants for aliased inputs.

#### Scenario: Vertical scanline alias
- **GIVEN** pass 1 declares `alias1 = "VERTICAL_SCANLINES"`
- **WHEN** pass 7 samples a sampler named `VERTICAL_SCANLINES`
- **THEN** the bound image SHALL be the output of pass 1
- **AND** `VERTICAL_SCANLINESSize` SHALL reflect the aliased texture size as vec4

### Requirement: Parameter Override Binding

The filter chain SHALL apply preset parameter overrides and populate shader parameters by name into push constants or UBO members.

#### Scenario: Override applied to UBO member
- **GIVEN** a shader defines parameter `mask_type` in its UBO
- **AND** the preset includes `mask_type = 2.0`
- **WHEN** the pass is recorded
- **THEN** the UBO member named `mask_type` SHALL be written with value `2.0`

### Requirement: Pass Input Mipmap Control

The filter chain SHALL honor `mipmap_inputN` when selecting sampler state for a pass input.

#### Scenario: Mipmap input enabled
- **GIVEN** a preset sets `mipmap_input11 = true`
- **WHEN** pass 11 samples `Source`
- **THEN** the sampler bound to `Source` SHALL have mipmapping enabled

### Requirement: DMA-BUF Import Uses Exported Plane Layout

The render backend SHALL import DMA-BUF textures using the plane layout metadata provided by the capture layer.

#### Scenario: Import explicit modifier + offset
- **GIVEN** the capture layer provides a DMA-BUF FD with `stride`, `offset`, and `modifier`
- **WHEN** the viewer imports the DMA-BUF via `VkImageDrmFormatModifierExplicitCreateInfoEXT`
- **THEN** the render backend SHALL set `VkSubresourceLayout.rowPitch` to the provided `stride`
- **AND** it SHALL set `VkSubresourceLayout.offset` to the provided `offset`
- **AND** it SHALL set `drmFormatModifier` to the provided `modifier`

### Requirement: Shader Caching
The system SHALL cache compiled RetroArch shaders to disk to minimize startup latency and eliminate redundant GPU work.

#### Scenario: Persistent Cache Lookup
- **GIVEN** a shader has been compiled once
- **WHEN** the same shader is requested again (even after app restart)
- **THEN** it SHALL be loaded from disk cache and Slang compilation SHALL be bypassed.

#### Scenario: Serialization of Reflection
- **GIVEN** a RetroArch shader requires complex bindings (UBOs, Textures, Push Constants)
- **WHEN** cached to disk
- **THEN** the cache MUST include full `ReflectionData` and it MUST be restored correctly on cache hit, including all binding offsets and stage flags.

#### Scenario: Automatic Invalidation
- **GIVEN** a cached shader exists
- **WHEN** the source code of that shader is modified
- **THEN** the system SHALL detect the hash mismatch and it SHALL recompile and update the cache.

#### Scenario: Type-Safe Serialization
- **GIVEN** data being serialized to disk
- **WHEN** using `write_pod` or `read_pod`
- **THEN** the system MUST enforce `std::is_standard_layout_v` to ensure memory safety for Vulkan-specific types like bitmasks and handles.

#### Scenario: Atomic Cache Updates
- **GIVEN** the system is writing a new cache file
- **WHEN** a crash or disk-full event occurs during the write
- **THEN** the existing valid cache file MUST NOT be corrupted
- **AND** the system SHALL use a temporary file and atomic rename to ensure cache integrity.

#### Scenario: Integrity Validation
- **GIVEN** a potentially corrupted cache file on disk
- **WHEN** the system attempts to load it
- **THEN** it SHALL validate SPIR-V alignment and header magic/version
- **AND** it SHALL discard the corrupted file and recompile the shader if validation fails.

#### Scenario: Minimal Log Output
- **GIVEN** the system is running at default log levels
- **WHEN** a cache hit occurs
- **THEN** it SHALL NOT output detailed per-parameter or diagnostic logs
- **AND** detailed information SHALL only be available at `TRACE` or `DEBUG` levels.

### Requirement: Present Wait Frame Pacing

The render backend SHALL use VK_KHR_present_wait when supported to pace presentation and avoid uncapped mailbox behavior on high-end GPUs.

#### Scenario: Present wait enabled
- **GIVEN** the physical device supports `VK_KHR_present_wait`
- **WHEN** the swapchain is created
- **THEN** the device SHALL enable the extension
- **AND** the present mode SHALL be `FIFO`
- **AND** the backend SHALL use present wait to pace to `render.target_fps`

#### Scenario: Uncapped target fps
- **GIVEN** `render.target_fps` is set to `0`
- **WHEN** present wait is available
- **THEN** the backend SHALL skip waiting for a target interval
- **AND** presentation SHALL proceed as fast as FIFO allows

#### Scenario: Present wait unsupported
- **GIVEN** `VK_KHR_present_wait` is not supported
- **WHEN** the swapchain is created
- **THEN** the backend SHALL prefer `MAILBOX` present mode
- **AND** it SHALL apply CPU-side frame capping when `render.target_fps` is non-zero
- **AND** it SHALL fall back to `FIFO` if `MAILBOX` is unavailable

#### Scenario: Target fps changes via config
- **GIVEN** a configuration file sets `render.target_fps` to a non-zero value
- **WHEN** the application starts
- **THEN** the backend SHALL pace presentation to that value

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

### Requirement: Post-Chain Infrastructure

The filter chain subsystem SHALL provide a generic post-chain stage that executes after RetroArch passes and before final swapchain presentation.

#### Scenario: Post-chain as vector of passes

- **GIVEN** `FilterChain` is initialized
- **THEN** it SHALL maintain `m_postchain_passes` as a vector of `Pass` pointers
- **AND** it SHALL maintain `m_postchain_framebuffers` as a vector of `Framebuffer` pointers
- **AND** the vectors SHALL have the same count (minus one for final output)

#### Scenario: Post-chain execution order

- **GIVEN** post-chain contains N passes
- **WHEN** `record_postchain()` is called
- **THEN** passes SHALL execute in vector order (0 to N-1)
- **AND** each pass output SHALL become the next pass input
- **AND** the final pass SHALL render to the swapchain

#### Scenario: OutputPass as final post-chain entry

- **GIVEN** `FilterChain` is initialized
- **THEN** `OutputPass` SHALL be added as the last entry in `m_postchain_passes`
- **AND** it SHALL always be present (minimum post-chain size is 1)
- **AND** no framebuffer SHALL be allocated for the final pass (renders to swapchain)

#### Scenario: Post-chain extensibility

- **GIVEN** a post-processing effect is needed after RetroArch passes
- **WHEN** a new pass is added to `m_postchain_passes` before OutputPass
- **THEN** it SHALL receive the RetroArch chain output as input
- **AND** its output SHALL be passed to subsequent post-chain passes

### Requirement: Pre-Chain Stage Infrastructure

The filter chain SHALL support a generic pre-chain stage that processes captured frames before the RetroArch shader passes. The pre-chain is a vector of passes, analogous to the RetroArch pass vector, allowing multiple preprocessing steps.

#### Scenario: Pre-chain as extensible pass vector

- **GIVEN** `FilterChain` is initialized
- **WHEN** pre-chain passes are configured
- **THEN** `m_prechain_passes` SHALL be a vector capable of holding multiple passes
- **AND** `m_prechain_framebuffers` SHALL be a vector of corresponding framebuffers
- **AND** passes SHALL execute in vector order

#### Scenario: Pre-chain disabled by default

- **GIVEN** no pre-chain passes are configured
- **WHEN** `FilterChain::record()` executes
- **THEN** captured frames SHALL pass directly to RetroArch passes (or OutputPass in passthrough mode)

#### Scenario: Pre-chain output becomes Original for RetroArch chain

- **GIVEN** pre-chain contains one or more passes
- **WHEN** `FilterChain::record()` executes
- **THEN** pre-chain passes SHALL execute first in vector order
- **AND** the final pre-chain output SHALL be used as `original_view` for RetroArch passes
- **AND** `OriginalSize` semantic SHALL reflect final pre-chain output dimensions

#### Scenario: Generic pre-chain recording

- **GIVEN** pre-chain contains N passes
- **WHEN** `record_prechain()` executes
- **THEN** each pass SHALL receive the previous pass's output as input
- **AND** image barriers SHALL be inserted between passes
- **AND** the loop SHALL NOT be hardcoded to a specific pass type

### Requirement: Downsample Pass

The internal pass library SHALL include an area-filter downsampling pass that can be added to the pre-chain.

#### Scenario: Area filter downsampling

- **GIVEN** source image at 1920x1080 and target resolution 640x480
- **WHEN** downsample pass executes
- **THEN** each output pixel SHALL be computed as a weighted average of covered source pixels
- **AND** the result SHALL exhibit minimal aliasing compared to point sampling

#### Scenario: Downsample added to pre-chain when configured

- **GIVEN** source resolution is configured via `--app-width` and/or `--app-height`
- **WHEN** `FilterChain` is created
- **THEN** a `DownsamplePass` SHALL be added to `m_prechain_passes`
- **AND** a framebuffer sized to target resolution SHALL be added to `m_prechain_framebuffers`

#### Scenario: Identity passthrough at same resolution

- **GIVEN** source and target resolution are identical
- **WHEN** downsample pass executes
- **THEN** output SHALL exactly match input
- **AND** no blurring or aliasing SHALL occur

### Requirement: Source Resolution CLI Semantics

The `--app-width` and `--app-height` CLI options SHALL configure the downsample pass in the pre-chain. Either option may be specified alone, with the other dimension calculated to preserve aspect ratio.

#### Scenario: Both dimensions specified

- **GIVEN** user specifies `--app-width 640 --app-height 480`
- **WHEN** Goggles starts
- **THEN** `DownsamplePass` SHALL be added to pre-chain with target 640x480

#### Scenario: Only width specified preserves aspect ratio

- **GIVEN** user specifies `--app-width 640` without `--app-height`
- **AND** captured frame is 1920x1080 (16:9 aspect ratio)
- **WHEN** first frame is processed
- **THEN** height SHALL be computed as `round(640 * 1080 / 1920) = 360`
- **AND** downsample pass target SHALL be 640x360

#### Scenario: Only height specified preserves aspect ratio

- **GIVEN** user specifies `--app-height 480` without `--app-width`
- **AND** captured frame is 1920x1080 (16:9 aspect ratio)
- **WHEN** first frame is processed
- **THEN** width SHALL be computed as `round(480 * 1920 / 1080) = 853`
- **AND** downsample pass target SHALL be 853x480

#### Scenario: Options still set environment variables

- **GIVEN** user specifies `--app-width` and/or `--app-height`
- **WHEN** target app is launched
- **THEN** `GOGGLES_WIDTH` and `GOGGLES_HEIGHT` environment variables SHALL be set for specified dimensions
- **AND** WSI proxy (if enabled) SHALL use these values for virtual surface sizing

### Requirement: Shader Stage UI Organization

The shader controls window SHALL organize controls into three collapsible sections corresponding to pipeline stages: Pre-Chain, Effect, and Post-Chain.

#### Scenario: Pre-chain section displays downsample controls

- **GIVEN** the shader controls window is visible
- **WHEN** the Pre-Chain section is expanded
- **THEN** resolution width and height input fields SHALL be displayed
- **AND** an "Apply" button SHALL be displayed to confirm changes

#### Scenario: Effect section displays RetroArch controls

- **GIVEN** the shader controls window is visible
- **WHEN** the Effect section is expanded
- **THEN** the shader enable checkbox SHALL be displayed
- **AND** the current preset label SHALL be displayed
- **AND** the available presets tree SHALL be displayed
- **AND** shader parameters SHALL be displayed if a preset is loaded

#### Scenario: Post-chain section displays placeholder

- **GIVEN** the shader controls window is visible
- **WHEN** the Post-Chain section is expanded
- **THEN** a label indicating "Output Blit" SHALL be displayed
- **AND** no controls SHALL be displayed

### Requirement: Pre-Chain Pipeline Configuration

The filter chain SHALL support runtime updates to pre-chain pipeline configuration (resolution) without requiring application restart. Pipeline configuration is distinct from shader parameters - it affects resource allocation and triggers pass rebuilds.

#### Scenario: Resolution update triggers pass rebuild

- **GIVEN** a pre-chain downsample pass exists
- **WHEN** `FilterChain::set_prechain_resolution(width, height)` is called with new values
- **THEN** existing pre-chain passes and framebuffers SHALL be cleared
- **AND** new passes SHALL be created on the next frame with the updated resolution

#### Scenario: Resolution query returns current state

- **GIVEN** a pre-chain resolution is configured
- **WHEN** `FilterChain::get_prechain_resolution()` is called
- **THEN** the current target width and height SHALL be returned

#### Scenario: Zero resolution disables pre-chain

- **GIVEN** pre-chain passes exist
- **WHEN** `set_prechain_resolution(0, 0)` is called
- **THEN** pre-chain processing SHALL be disabled
- **AND** captured frames SHALL pass directly to effect stage

### Requirement: Pre-Chain UI State Synchronization

The UI layer SHALL maintain synchronized state with the filter chain pre-chain configuration.

#### Scenario: UI initialized from backend state

- **GIVEN** the application starts with `--app-width 640 --app-height 480`
- **WHEN** the ImGui layer is initialized
- **THEN** the pre-chain resolution inputs SHALL display 640 and 480

#### Scenario: UI callback propagates changes

- **GIVEN** the pre-chain section is visible
- **WHEN** the user changes resolution and clicks Apply
- **THEN** the pre-chain change callback SHALL be invoked
- **AND** the new resolution SHALL be passed to the filter chain

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

### Requirement: Runtime Shader Preset Reload
The render pipeline SHALL support rebuilding the RetroArch filter chain at runtime when the application requests a new `.slangp` preset without forcing an application restart.

#### Scenario: UI-triggered reload success
- **GIVEN** the Shader Controls panel emits a preset selection event containing `<path>`
- **WHEN** the render pipeline handles the event
- **THEN** it SHALL wait for the in-flight frame to complete
- **AND** it SHALL destroy the current filter chain state
- **AND** it SHALL parse and instantiate the new preset from `<path>`
- **AND** the next frame SHALL render using the newly loaded passes

#### Scenario: Reload failure fallback
- **GIVEN** the render pipeline attempts to load a preset that fails to parse or compile
- **WHEN** the failure occurs
- **THEN** the previously active preset SHALL remain bound and continue rendering
- **AND** the failure SHALL be reported back to the UI/log so the user can pick a different preset

### Requirement: Passthrough Mode Toggle
The render pipeline SHALL provide a passthrough mode that bypasses all filter passes and blits the captured frame directly when requested, while remembering the last successful preset for restoration.

#### Scenario: Passthrough enabled at runtime
- **GIVEN** the Shader Controls panel requests passthrough mode
- **WHEN** the render pipeline processes the request
- **THEN** it SHALL stop invoking the filter chain and route the captured texture directly into `OutputPass`
- **AND** no RetroArch preset compilation SHALL occur while passthrough is active

#### Scenario: Passthrough disabled restores preset
- **GIVEN** passthrough mode is active and the user turns it off
- **WHEN** the render pipeline receives the request
- **THEN** it SHALL reload the last successful preset (or the default from config if none exists)
- **AND** rendering SHALL resume using the restored filter chain without requiring an application restart

### Requirement: Runtime Parameter Access
The render pipeline SHALL expose shader parameter metadata and runtime override capabilities so the UI layer can display and modify filter chain parameters.

#### Scenario: Parameter list query
- **GIVEN** a filter chain with one or more passes is loaded
- **WHEN** the UI queries available parameters
- **THEN** the pipeline SHALL return a list of ShaderParameter (name, description, min, max, step, default, current value)
- **AND** parameters from all passes SHALL be accessible

#### Scenario: Parameter override
- **GIVEN** a valid parameter name and value within bounds
- **WHEN** set_parameter_override(name, value) is called
- **THEN** the filter chain SHALL apply the override to the appropriate pass
- **AND** update_ubo_parameters() SHALL be invoked before the next render

#### Scenario: Parameter reset
- **GIVEN** one or more parameters have been overridden
- **WHEN** clear_parameter_overrides() is called
- **THEN** all overrides SHALL be removed
- **AND** parameters SHALL revert to preset defaults on the next frame

### Requirement: Render Scale Mode Ownership

The render backend SHALL own the active scale mode and integer scale values and expose them for application and UI synchronization.

#### Scenario: Query returns current runtime state

- **GIVEN** the active scale mode has been updated at runtime
- **WHEN** the application queries the render backend for the active scale mode
- **THEN** the backend SHALL return the updated mode
- **AND** the current integer scale SHALL be available alongside it

### Requirement: Runtime Scale Mode Switching

The viewer SHALL allow switching the render scale mode at runtime and apply changes to subsequent frames without restart.

#### Scenario: UI change updates active scale mode

- **GIVEN** the shader controls window is visible
- **WHEN** the user selects a new scale mode in the Pre-Chain section
- **THEN** the active render scale mode SHALL update without restart
- **AND** subsequent frames SHALL use the new mode

#### Scenario: Dynamic mode request uses active backend state

- **GIVEN** the capture receiver is connected
- **WHEN** the active scale mode is `dynamic` and the swapchain extent changes or dynamic mode becomes active
- **THEN** the viewer SHALL request the source resolution to match the swapchain extent
- **AND** no request SHALL be sent when the active scale mode is not `dynamic`

### Requirement: Pre-Chain Scale Mode Controls

The Pre-Chain stage UI SHALL expose controls for the viewer scale mode.

#### Scenario: Scale mode selector is available

- **GIVEN** the shader controls window is visible
- **WHEN** the Pre-Chain section is expanded
- **THEN** a scale mode selector SHALL be displayed
- **AND** it SHALL include fit, fill, stretch, integer, and dynamic options

#### Scenario: Integer scale input visibility

- **GIVEN** the scale mode selector is set to `integer`
- **WHEN** the Pre-Chain section is visible
- **THEN** an integer scale input SHALL be displayed
- **AND** changes SHALL update the active integer scale at runtime

### Requirement: Per-Surface Filter Chain Routing
The render pipeline SHALL honor a per-surface filter-chain enable flag and a session-wide global
enable flag when deciding whether to execute the filter chain for a frame.

The effect stage SHALL also respect `Shader Controls -> Effect Stage (RetroArch) -> Enable Shader`.

When the global flag is disabled, the pipeline SHALL bypass the filter chain for all surfaces and
render captured surfaces using a compositor-style maximize resize so the client re-renders at the
window size (no stretch-blit).

When the global flag is enabled but the per-surface flag is disabled, the pipeline SHALL bypass the
filter chain for that surface and render it using a compositor-style maximize resize so the client
re-renders at the window size (no stretch-blit).

The per-surface mode SHALL apply to the entire xdg_toplevel surface, including all popups and subsurfaces belonging to that toplevel.

#### Scenario: Default uses filter chain
- **GIVEN** a surface has no explicit override
- **WHEN** a frame is rendered for that surface
- **THEN** the filter chain SHALL be executed for that frame

#### Scenario: Bypass filter chain for a surface
- **GIVEN** a surface has filter-chain disabled
- **WHEN** a frame is rendered for that surface
- **THEN** the filter chain SHALL be bypassed
- **AND** the surface SHALL be rendered via a maximize-style resize without stretch-blit

#### Scenario: Global bypass overrides per-surface
- **GIVEN** the global filter-chain flag is disabled
- **WHEN** a frame is rendered for any surface
- **THEN** the filter chain SHALL be bypassed
- **AND** the surface SHALL be rendered via a maximize-style resize without stretch-blit

#### Scenario: Popup inherits parent mode
- **GIVEN** an xdg_toplevel surface has filter-chain disabled
- **WHEN** a popup or subsurface belonging to that toplevel is rendered
- **THEN** the popup SHALL be rendered with filter-chain bypass and maximize-style resize without stretch-blit

### Requirement: Pass Initialization Interface

All render pass classes SHALL use a consistent initialization pattern with typed config structs and shared Vulkan context.

#### Scenario: VulkanContext sharing

- **GIVEN** `VulkanBackend` has initialized device and physical device
- **WHEN** any pass is initialized
- **THEN** the pass SHALL receive a `VulkanContext` reference containing both handles
- **AND** the pass SHALL NOT store redundant copies of device handles

#### Scenario: OutputPass initialization with config

- **GIVEN** an `OutputPassConfig` with target format, sync indices, and shader directory
- **WHEN** `OutputPass::init()` is called with `VulkanContext`, `ShaderRuntime`, and config
- **THEN** the pass SHALL initialize using the provided configuration
- **AND** the signature SHALL be `init(const VulkanContext&, ShaderRuntime&, const OutputPassConfig&)`

#### Scenario: FilterPass initialization with config

- **GIVEN** a `FilterPassConfig` with target format, sync indices, shader sources, and filter mode
- **WHEN** `FilterPass::init()` is called with `VulkanContext`, `ShaderRuntime`, and config
- **THEN** the pass SHALL compile shaders and create pipeline from the config
- **AND** the signature SHALL be `init(const VulkanContext&, ShaderRuntime&, const FilterPassConfig&)`

