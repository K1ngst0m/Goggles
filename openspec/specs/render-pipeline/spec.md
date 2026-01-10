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

