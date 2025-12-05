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

## References

- RetroArch: `gfx/drivers_shader/shader_vulkan.cpp`
- Slang spec: https://docs.libretro.com/development/shader/slang-shaders/
- librashader: https://github.com/SnowflakePowered/librashader
