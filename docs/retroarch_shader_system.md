# RetroArch Shader Processing System Research

## Overview

This document summarizes the RetroArch shader system architecture based on analysis of:
- RetroArch native implementation (`gfx/drivers_shader/shader_vulkan.cpp`)
- Slang shader specification (libretro docs)
- librashader standalone implementation (Rust)

## Core Concepts

### Filter Chain

A filter chain is a directed graph of shader passes:

```
(Input) → [Pass 0] → (FB0) → [Pass 1] → (FB1) → ... → [Pass N] → (Backbuffer)
```

Key properties:
- Each pass renders a fullscreen quad to a framebuffer
- All passes can access any previous pass output
- History textures provide access to previous frames
- Feedback textures enable IIR-style effects

### Data Flow Per Frame

```
┌─────────────────────────────────────────────────────────────────┐
│ Frame N                                                         │
├─────────────────────────────────────────────────────────────────┤
│ Inputs available to any pass:                                   │
│   - Original: Current frame input                               │
│   - OriginalHistory[1..N]: Previous N frames of input           │
│   - Source: Previous pass output (or Original for pass 0)       │
│   - PassOutput[0..N-1]: Output of specific earlier pass         │
│   - PassFeedback[0..N]: Previous frame's pass outputs           │
│   - User LUTs: Static lookup textures                           │
└─────────────────────────────────────────────────────────────────┘
```

---

## RetroArch Native Architecture (Vulkan)

### Key Classes

| Class | Responsibility |
|-------|----------------|
| `vulkan_filter_chain` | Owns all passes, shared resources, history/feedback buffers |
| `Pass` | Single shader pass: pipeline, descriptors, uniform buffer |
| `CommonResources` | Shared VBO, samplers, LUT textures, history storage |
| `Framebuffer` | VkImage + VkFramebuffer + VkRenderPass for a pass output |

### vulkan_filter_chain Structure

```cpp
struct vulkan_filter_chain {
    std::vector<std::unique_ptr<Pass>> passes;
    CommonResources common;
    
    // Per-pass framebuffers
    std::vector<std::unique_ptr<Framebuffer>> framebuffers;
    
    // History ring buffer (previous frames of Original)
    std::vector<vulkan_filter_chain_texture> original_history;
    
    // Output textures from each pass (for PassOutput# access)
    std::vector<vulkan_filter_chain_texture> pass_outputs;
    
    // Deferred resource cleanup
    std::vector<std::function<void()>> deferred_calls;
};
```

### Pass Class

```cpp
class Pass {
    // Compiled shaders
    std::vector<uint32_t> vertex_shader;   // SPIR-V
    std::vector<uint32_t> fragment_shader; // SPIR-V

    // Vulkan resources
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout set_layout;
    VkDescriptorPool pool;
    std::vector<VkDescriptorSet> sets;     // Per sync-index (frame-in-flight)

    // Per-frame tracking
    unsigned sync_index;                    // Current frame index (set by FilterChain)
    unsigned num_sync_indices;              // Total frames in flight (from swapchain)

    // Reflection data
    slang_reflection reflection;

    // Output framebuffer (null for final pass)
    std::unique_ptr<Framebuffer> framebuffer;
    std::unique_ptr<Framebuffer> fb_feedback; // For PassFeedback
};
```

### Pass Descriptor Set Management

**Key insight:** Each Pass owns `num_sync_indices` descriptor sets (one per frame-in-flight).

**Allocation (in `Pass::init_pipeline_layout()`):**
```cpp
// Pool sized for all sync indices
pool_info.maxSets = num_sync_indices;
// Each resource type multiplied by sync indices
pool_size.descriptorCount = num_sync_indices;  // per binding type

// Allocate one set per sync index
sets.resize(num_sync_indices);
for (i = 0; i < num_sync_indices; i++)
    vkAllocateDescriptorSets(device, &alloc_info, &sets[i]);
```

**Usage (in `Pass::build_commands()`):**
```cpp
// FilterChain notifies current frame index via notify_sync_index()
// Pass updates the descriptor set for this frame
build_semantics(sets[sync_index], ...);

// UBO offset is also per-sync-index
ubo_offset + sync_index * common->ubo_sync_index_stride

// Bind the correct set during rendering
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipeline_layout, 0, 1, &sets[sync_index], 0, nullptr);
```

**Why per-frame descriptor sets:**
- Avoids VUID-vkUpdateDescriptorSets-None-03047 (updating descriptor while GPU reads)
- Each frame's descriptor set can be safely updated while previous frame is still rendering
- FilterChain broadcasts sync_index to all passes, ensuring consistent frame selection

### Execution Flow

```
vulkan_filter_chain::build_offscreen_passes(cmd):
    for each pass except final:
        pass.build_commands(cmd, original, source)
        source = pass.get_output()  // Chain outputs
        
vulkan_filter_chain::build_viewport_pass(cmd, viewport):
    final_pass.build_commands(cmd, original, source)
    // Renders directly to swapchain
```

---

## Shader Compilation Pipeline

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  .slang file │ →  │   glslang    │ →  │   SPIR-V     │
│              │    │  (compiler)  │    │   binary     │
└──────────────┘    └──────────────┘    └──────────────┘
                                               │
                                               ▼
                                        ┌──────────────┐
                                        │  SPIRV-Cross │
                                        │ (reflection) │
                                        └──────────────┘
                                               │
                                               ▼
                                        ┌──────────────┐
                                        │ slang_reflect│
                                        │ (semantics)  │
                                        └──────────────┘
```

### Reflection Output

```cpp
struct slang_reflection {
    size_t ubo_size;
    unsigned ubo_binding;
    size_t push_constant_size;
    
    // Built-in semantic locations
    slang_semantic_meta semantics[SLANG_NUM_SEMANTICS];
    
    // Texture semantic locations  
    slang_texture_semantic_meta semantic_textures[SLANG_NUM_TEXTURE_SEMANTICS][8];
    
    // User parameters
    std::vector<slang_semantic_meta> semantic_float_parameters;
};
```

---

## Shader Semantics

### Built-in Uniforms

| Semantic | Type | Description |
|----------|------|-------------|
| `MVP` | mat4 | Model-View-Projection matrix |
| `OutputSize` | vec4 | (width, height, 1/width, 1/height) of current pass output |
| `FinalViewportSize` | vec4 | Final backbuffer size |
| `FrameCount` | uint | Frame counter |
| `FrameDirection` | int | 1 or -1 (rewind) |
| `SourceSize` | vec4 | Size of Source texture |
| `OriginalSize` | vec4 | Size of Original texture |

### Texture Semantics

| Semantic | Description |
|----------|-------------|
| `Original` | Input to filter chain (captured frame) |
| `Source` | Previous pass output (or Original for pass 0) |
| `OriginalHistory#` | Input from # frames ago |
| `PassOutput#` | Output from pass # this frame |
| `PassFeedback#` | Output from pass # previous frame |
| `User#` | LUT texture |

---

## Preset Format (.slangp)

```ini
shaders = 3

shader0 = shaders/pass0.slang
scale_type0 = source
scale0 = 2.0
filter_linear0 = false
float_framebuffer0 = true

shader1 = shaders/pass1.slang  
scale_type1 = viewport
filter_linear1 = true
srgb_framebuffer1 = true

shader2 = shaders/final.slang
# No scale = outputs to backbuffer

# LUT textures
textures = "LUT1"
LUT1 = luts/color.png
LUT1_linear = true

# Parameters
parameters = "brightness;contrast"
brightness = 1.0
contrast = 1.0
```

### Scale Types

| Type | Meaning |
|------|---------|
| `source` | Scale relative to pass input |
| `viewport` | Scale relative to final viewport |
| `absolute` | Fixed pixel dimensions |

### Framebuffer Formats

- `R8G8B8A8_UNORM` (default)
- `R8G8B8A8_SRGB`
- `R16G16B16A16_SFLOAT`
- `A2B10G10R10_UNORM_PACK32`
- etc.

---

## librashader Architecture (Rust)

librashader is a standalone implementation that can be used outside RetroArch.

### Key Structures

```rust
struct FilterChainVulkan {
    pub(crate) common: FilterCommon,
    pub(crate) passes: Box<[FilterPass]>,
    pub(crate) output_framebuffers: Box<[OwnedImage]>,
    pub(crate) feedback_framebuffers: Box<[OwnedImage]>,
    pub(crate) history_framebuffers: VecDeque<OwnedImage>,
    pub(crate) residuals: Box<[FrameResiduals]>,  // Per frame-in-flight
}

struct FilterCommon {
    pub samplers: SamplerSet,
    pub luts: FastHashMap<usize, LutTexture>,
    pub draw_quad: DrawQuad,
    pub output_textures: Box<[Option<InputImage>]>,
    pub feedback_textures: Box<[Option<InputImage>]>,
    pub history_textures: Box<[Option<InputImage>]>,
    pub config: RuntimeParameters,
}

struct FilterPass {
    pub reflection: ShaderReflection,
    pub uniform_storage: UniformStorage<RawVulkanBuffer>,
    pub graphics_pipeline: VulkanGraphicsPipeline,
    pub meta: PassMeta,
}
```

### Loading Modes

1. **Immediate**: Creates command buffer, submits, waits
2. **Deferred**: Caller provides command buffer, records commands, caller submits

### Frame Residuals

Resources that must survive until GPU completes:
- Image views created during frame
- Temporary framebuffers
- Owned images being replaced

Managed via ring buffer indexed by `frame_count % frames_in_flight`.

### History Buffer Management

```rust
// Each frame:
let old_history = history_framebuffers.pop_back();
copy_input_to(old_history);
history_framebuffers.push_front(old_history);
// Now history[0] = current input, history[1] = previous, etc.
```

### Feedback Buffer Swap

```rust
// Start of frame:
std::mem::swap(&mut output_framebuffers, &mut feedback_framebuffers);
// Now feedback_framebuffers has previous frame's outputs
// output_framebuffers will receive this frame's outputs
```

---

## Design Implications for Goggles

### Module Boundaries

Based on RetroArch/librashader, module structure:

```
src/render/
├── backend/                # Vulkan API abstraction
│   ├── vulkan_backend.hpp  # Device, swapchain, queues
│   └── vulkan_config.hpp   # Vulkan types/includes
├── shader/                 # Slang compilation, SPIR-V caching, reflection
│   └── shader_runtime.hpp  # Slang → SPIR-V
└── chain/                  # Filter chain (RetroArch-style)
    ├── filter_chain.hpp    # Main filter chain class
    ├── filter_pass.hpp     # Single pass
    ├── semantics.hpp       # Semantic definitions
    └── preset_parser.hpp   # .slangp parsing
```

### Key Abstractions

1. **ShaderRuntime**: Compile Slang → SPIR-V, cache, reflect
2. **FilterPass**: One pass with pipeline, descriptors, framebuffer
3. **FilterChain**: Orchestrates passes, manages history/feedback
4. **SemanticBinder**: Populates uniforms from frame state

### Current Blit Pass Position

The current `BlitPipeline` (passthrough blit) should become:
- **Output stage** when RetroArch passes are added
- Lives at the END of the filter chain
- Converts from working format (RGBA16F) to swapchain format

When RetroArch is added, a new **input normalization** stage is needed:
- Lives at the START of the filter chain
- Converts captured image to working format (RGBA16F linear)
- This becomes the "Original" texture for RetroArch semantics

### Resource Lifetime Considerations

1. **Per-frame-in-flight resources**: Descriptor sets, uniform buffers
2. **Residual tracking**: Resources created during frame must survive GPU completion
3. **History ring buffer**: Fixed size, oldest replaced each frame
4. **Feedback swap**: Double-buffered per pass

### Vulkan-hpp Compatibility

RetroArch uses raw Vulkan C API. For Goggles (per PROJECT_POLICIES.md D.2):
- Use vulkan-hpp with `vk::Unique*` for pipelines, layouts
- Plain handles for per-frame resources (descriptors, command buffers)
- Explicit sync before destroying GPU-async resources

---

## Critical Divergence: Compiler and Syntax

### The "Slang" Naming Confusion

**IMPORTANT:** The term "Slang" refers to two completely different things:

| Term | What it is | Used by |
|------|-----------|---------|
| **RetroArch "Slang"** | File format specification for Vulkan GLSL with custom pragmas | RetroArch, librashader |
| **Microsoft Slang** | A shading language by Microsoft/NVIDIA (HLSL-based) | Goggles (current) |

Goggles currently uses **Microsoft Slang** with HLSL-style syntax, but RetroArch shaders use **Vulkan GLSL** (the "Slang" format).

### Compiler Comparison

| Aspect | Goggles (Current) | RetroArch |
|--------|------------------|-----------|
| **Compiler** | Microsoft Slang (`slang.h`) | glslang (Khronos) |
| **Base Language** | HLSL-style | GLSL (#version 450) |
| **Shader Stage** | `[shader("vertex")]` attribute | `#pragma stage vertex` |
| **Entry Point** | Named function with attribute | `void main()` |
| **Types** | `float4`, `float2`, `Sampler2D` | `vec4`, `vec2`, `sampler2D` |
| **Texture Sampling** | `.Sample()` method | `texture()` function |
| **Descriptor Binding** | `[[vk::binding(0,0)]]` | `layout(set=0, binding=0)` |

### Current Goggles Shader Example

```hlsl
// blit.frag.slang (HLSL-style)
[[vk::binding(0, 0)]]
Sampler2D source_texture;

[shader("pixel")]
float4 main(float2 texcoord : TEXCOORD0) : SV_Target0 {
    return source_texture.Sample(texcoord);
}
```

### RetroArch Shader Example

```glsl
// zfast_crt.slang (GLSL-style)
#version 450
layout(push_constant) uniform Push {
    vec4 SourceSize;
    vec4 OutputSize;
    float BRIGHTNESS;
} params;

#pragma parameter BRIGHTNESS "Brightness" 1.0 0.5 2.0 0.05

layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main() {
   gl_Position = global.MVP * Position;
   vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;
void main() {
   FragColor = texture(Source, vTexCoord) * params.BRIGHTNESS;
}
```

---

## RetroArch Shader Preprocessing Requirements

RetroArch/glslang does **NOT** support these directives natively. A custom preprocessor is required:

### Custom Pragmas (Extract, Don't Compile)

| Pragma | Purpose | Preprocessing Action |
|--------|---------|---------------------|
| `#pragma stage vertex/fragment` | Split stages | Split source at pragma, compile each stage separately |
| `#pragma parameter NAME "Desc" default min max step` | User-tunable uniforms | Extract to metadata, verify uniform exists |
| `#pragma name IDENTIFIER` | Pass alias | Extract to metadata for semantic binding |
| `#pragma format FORMAT` | Framebuffer format | Extract to pass configuration |

### `#include` Handling

- Must be resolved **before** compilation (glslang doesn't support `#include` by default)
- Paths are relative to the including file
- No cycle detection (undefined behavior if cyclic)
- Common include files in RetroArch shaders:
  - `include/compat_macros.inc` - HLSL→GLSL type mapping (`float2` → `vec2`)
  - Pass-specific headers (e.g., `bind-shader-params.h`)

### Conditional Compilation (`#ifdef`, `#define`)

- Used extensively in complex shaders (e.g., crt-royale)
- Some defines control runtime behavior, others are compile-time
- Example: `#ifdef RUNTIME_SHADER_PARAMS_ENABLE` toggles between static constants and uniforms

---

## Shader Complexity Analysis

### Simple Shader: zfast-crt

**Characteristics:**
- Single pass
- Simple preprocessing: only `#pragma parameter` extraction needed
- Self-contained (no `#include`)
- Uses push constants for all parameters
- Straightforward translation possible

**Key Patterns:**
```glsl
// Push constants for per-frame data
layout(push_constant) uniform Push {
    vec4 SourceSize;
    float PARAM1, PARAM2;
} params;

// UBO for MVP only
layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;

// Source texture always at binding 2
layout(set = 0, binding = 2) uniform sampler2D Source;
```

### Complex Shader: crt-royale

**Characteristics:**
- 9 passes with different scale types (source, viewport, absolute)
- Heavy `#include` usage (17+ header files)
- 6 LUT textures (mask textures)
- Pass aliasing (e.g., `ORIG_LINEARIZED`, `HALATION_BLUR`)
- Conditional compilation for feature toggles
- ~50 user parameters
- Some passes reference multiple previous pass outputs

**Preprocessing Challenges:**
1. Deep `#include` tree must be flattened
2. Conditional compilation based on defines from both `.slangp` and headers
3. Large UBO (exceeds 128 bytes, requires spill to regular UBO)
4. Header files redefine compatibility macros (`float2` → `vec2`, etc.)

---

## Translation Strategy Options

### Option 1: Slang GLSL Mode + Preprocessing (SELECTED)

**Approach:**
1. Enable Slang's GLSL compatibility mode (`enableGLSL = true`, `allowGLSLSyntax = true`)
2. Add lightweight text preprocessing for RetroArch-specific pragmas:
   - Resolve `#include` (Slang supports this natively via search paths)
   - Extract `#pragma parameter/name/format` as metadata
   - Split source at `#pragma stage vertex/fragment`
3. Compile with Slang to SPIR-V (single compiler)
4. Use SPIRV-Cross for reflection

**Pros:**
- Single compiler dependency (Slang already used for native shaders)
- Slang is actively maintained by NVIDIA/Khronos
- GLSL mode well-supported (Vulkan SDK includes Slang since 1.3.296)
- Preprocessing is text-only (simple, no compiler complexity)

**Cons:**
- May encounter edge cases in complex shaders (mitigated by incremental testing)

**Decision:** This is the selected approach. See `openspec/changes/add-retroarch-shader-support/` for implementation details.

### Option 2: glslang (Rejected)

**Approach:**
- Add glslang as second compiler for RetroArch shaders

**Rejected because:**
- Two compilers = double maintenance burden
- If we add glslang, why keep Slang at all?
- Slang GLSL mode provides equivalent functionality

### Option 3: Static Translation (Rejected)

**Approach:**
- One-time conversion of RetroArch shaders to Slang HLSL syntax

**Rejected because:**
- Maintenance burden (RetroArch shaders update frequently)
- Translation errors likely for complex shaders
- Would need ongoing translation effort for new shaders

---

## Preprocessor Implementation Outline

```
┌────────────────────┐
│ .slang file        │
└────────┬───────────┘
         │
         ▼
┌────────────────────┐
│ Include Resolution │  ← Recursive file reading
└────────┬───────────┘
         │
         ▼
┌────────────────────┐
│ Pragma Extraction  │  ← #pragma parameter, name, format
└────────┬───────────┘
         │
         ▼
┌────────────────────┐
│ Stage Splitting    │  ← #pragma stage vertex/fragment
└────────┬───────────┘
         │
         ▼
┌────────────────────┐     ┌─────────────────┐
│ glslang Compile    │ ──► │ SPIR-V (vertex) │
│ (per stage)        │     │ SPIR-V (frag)   │
└────────────────────┘     └────────┬────────┘
                                    │
                                    ▼
                           ┌─────────────────┐
                           │ SPIRV-Cross     │
                           │ (reflection)    │
                           └────────┬────────┘
                                    │
                                    ▼
                           ┌─────────────────┐
                           │ Semantic Binding│
                           │ (UBO/Push/Tex)  │
                           └─────────────────┘
```

### Preprocessing Pseudo-code

```cpp
struct PreprocessedShader {
    std::string vertex_source;
    std::string fragment_source;
    std::vector<Parameter> parameters;
    std::string name_alias;
    vk::Format format = vk::Format::eR8G8B8A8Unorm;
};

auto preprocess_slang(const fs::path& path) -> PreprocessedShader {
    // 1. Read file
    std::string source = read_file(path);

    // 2. Resolve includes (recursive)
    source = resolve_includes(source, path.parent_path());

    // 3. Extract pragmas
    auto parameters = extract_parameters(source);
    auto name = extract_name(source);
    auto format = extract_format(source);

    // 4. Split by stage
    auto [vertex, fragment] = split_by_stage(source);

    return { vertex, fragment, parameters, name, format };
}
```

---

## Integration with Current Goggles Architecture

### ShaderRuntime Extension

Current `ShaderRuntime` compiles Microsoft Slang. Extend with:

```cpp
class ShaderRuntime {
public:
    // Existing: Microsoft Slang compilation
    auto compile_shader(const fs::path& path) -> Result<CompiledShader>;

    // New: RetroArch shader compilation
    auto compile_retroarch_shader(const fs::path& path)
        -> Result<RetroArchShader>;

    struct RetroArchShader {
        std::vector<uint32_t> vertex_spirv;
        std::vector<uint32_t> fragment_spirv;
        std::vector<Parameter> parameters;
        std::string name;
        vk::Format format;
        ReflectionData reflection;
    };
};
```

### FilterPass Updates

The existing `OutputPass` needs to evolve:

```cpp
class FilterPass : public Pass {
    // RetroArch-specific
    std::string m_name_alias;           // #pragma name
    vk::Format m_framebuffer_format;    // #pragma format
    std::vector<Parameter> m_parameters;

    // Uniform data
    std::vector<uint8_t> m_ubo_data;
    std::vector<uint8_t> m_push_data;

    // Framebuffer (null for final pass)
    std::unique_ptr<Framebuffer> m_framebuffer;
};
```

---

## Open Questions

1. **Parameter UI**: How will user-adjustable parameters be exposed?
   - Separate config file?
   - Runtime GUI?
   - Command-line overrides?

2. **Hot Reload**: Should shader changes be detected at runtime?
   - Development convenience vs. complexity

3. **Preset Switching**: How to handle .slangp loading/unloading?
   - Full chain rebuild on preset change?
   - Cache compiled passes?

4. **History Buffer Size**: Per-preset or global limit?
   - RetroArch: arbitrary, shader-defined
   - Memory implications for high-resolution capture

---

## Implementation Status

The following components have been implemented as part of the RetroArch shader support:

### Completed Components

| Component | Location | Description |
|-----------|----------|-------------|
| **ShaderRuntime GLSL Mode** | `src/render/shader/shader_runtime.cpp` | Dual session (HLSL + GLSL) with Slang compiler |
| **RetroArch Preprocessor** | `src/render/shader/retroarch_preprocessor.cpp` | Stage splitting, parameter extraction, include resolution |
| **Preset Parser** | `src/render/chain/preset_parser.cpp` | INI-style `.slangp` file parsing |
| **Slang Reflection** | `src/render/shader/slang_reflect.cpp` | Native Slang reflection API (no SPIRV-Cross needed) |
| **Semantic Binder** | `src/render/chain/semantic_binder.hpp` | UBO/push constant population for RetroArch semantics |
| **FilterPass** | `src/render/chain/filter_pass.cpp` | Single pass rendering with RetroArch shader support |

### Key Implementation Details

#### Slang GLSL Mode Configuration

```cpp
// Global session with GLSL support
SlangGlobalSessionDesc global_desc = {};
global_desc.enableGLSL = true;
slang::createGlobalSession(&global_desc, global_session.writeRef());

// GLSL session for RetroArch shaders
slang::SessionDesc glsl_session_desc = {};
glsl_session_desc.allowGLSLSyntax = true;
// ... target setup for SPIRV output
```

#### Entry Point Discovery

GLSL shaders don't have `[shader(...)]` attributes, so use `findAndCheckEntryPoint`:

```cpp
SlangStage slang_stage = (stage == ShaderStage::VERTEX)
    ? SLANG_STAGE_VERTEX : SLANG_STAGE_FRAGMENT;
module->findAndCheckEntryPoint("main", slang_stage,
    entry_point.writeRef(), diagnostics.writeRef());
```

#### Reflection API

Using Slang's native reflection instead of SPIRV-Cross:

```cpp
slang::ProgramLayout* layout = linked->getLayout();
for (unsigned i = 0; i < layout->getParameterCount(); i++) {
    auto param = layout->getParameterByIndex(i);
    auto category = param->getCategory();
    // PushConstantBuffer, DescriptorTableSlot, etc.
}
```

### Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_shader_runtime.cpp` | GLSL compilation, error handling |
| `test_retroarch_preprocessor.cpp` | Stage splitting, parameter extraction, includes |
| `test_preset_parser.cpp` | INI parsing, multi-pass presets |
| `test_slang_reflect.cpp` | UBO, push constants, texture bindings |
| `test_semantic_binder.cpp` | Size vec4 computation, MVP matrix |
| `test_zfast_integration.cpp` | End-to-end zfast-crt-composite verification |

### Verified Shaders

- **zfast-crt-composite**: Single pass CRT shader, compiles and reflects correctly
  - Push constants: SourceSize, OutputSize, OriginalSize, FrameCount + 10 parameters
  - UBO: MVP matrix at binding 0
  - Texture: Source at binding 2

### Future Work (Out of Scope)

- **FilterChain Orchestration**: Multi-pass execution with framebuffer management
- **History Buffers**: OriginalHistory# texture access
- **Feedback Buffers**: PassFeedback# for IIR effects
- **LUT Textures**: Static lookup textures from presets
- **Parameter UI**: Runtime parameter adjustment

---

## References

- RetroArch: `gfx/drivers_shader/shader_vulkan.cpp`
- Slang spec: https://docs.libretro.com/development/shader/slang-shaders/
- librashader: https://github.com/SnowflakePowered/librashader
- Microsoft Slang: https://github.com/shader-slang/slang
- glslang: https://github.com/KhronosGroup/glslang
- Compatibility macros: `slang-shaders/include/compat_macros.inc`
