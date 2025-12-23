## Context

Goggles is a real-time frame capture and post-processing application with strict latency requirements (<16.6ms per frame for 60fps). Understanding where time is spent in the render loop, filter chain, and capture pipeline is essential for optimization. Current debugging relies on log timestamps and manual analysis, which is insufficient for frame-level profiling.

**Stakeholders:** Developers optimizing render performance, maintainers debugging latency issues.

## Goals / Non-Goals

**Goals:**
- Provide zero-overhead profiling when disabled (compile-time elimination)
- Enable detailed frame-by-frame timing analysis via Tracy UI
- Abstract profiling behind macros to allow future backend swaps (chrono, GPU timestamps)
- Instrument all performance-critical code paths identified in the codebase analysis

**Non-Goals:**
- GPU profiling via Vulkan timestamp queries (future work)
- Automated performance regression testing
- Production telemetry or metrics collection

## Decisions

### Decision 1: Use Tracy via CPM.cmake

**What:** Integrate Tracy v0.11.1+ using CPM.cmake with `TRACY_ENABLE` compile definition controlled by `ENABLE_PROFILING` CMake option.

**Why:**
- Tracy is the industry standard for game/graphics profiling
- Header-only core with minimal integration overhead
- Excellent Vulkan support (GPU zones, memory tracking)
- CPM.cmake aligns with project dependency policy (Section G)

**Alternatives considered:**
- Optick: Less actively maintained, similar feature set
- Superluminal: Windows-only
- Manual chrono timing: Insufficient visibility, no UI

### Decision 2: Macro Abstraction Layer

**What:** Create `src/util/profiling.hpp` with macros that wrap Tracy calls:

```cpp
// When ENABLE_PROFILING is ON:
#define GOGGLES_PROFILE_SCOPE(name)     ZoneScopedN(name)
#define GOGGLES_PROFILE_FUNCTION()      ZoneScoped
#define GOGGLES_PROFILE_FRAME(name)     FrameMarkNamed(name)
#define GOGGLES_PROFILE_BEGIN(name)     /* TracyZoneBegin equivalent */
#define GOGGLES_PROFILE_END()           /* TracyZoneEnd equivalent */
#define GOGGLES_PROFILE_TAG(text)       ZoneText(text, strlen(text))
#define GOGGLES_PROFILE_VALUE(name, v)  TracyPlot(name, v)

// When ENABLE_PROFILING is OFF:
// All macros expand to nothing (zero overhead)
```

**Why:**
- Allows swapping profiling backend without touching instrumented code
- Enables lightweight `std::chrono` fallback for environments without Tracy server
- Follows project pattern established by `GOGGLES_TRY`/`GOGGLES_MUST` macros

### Decision 3: clang-tidy Suppression for Profiling Macros

**What:** Add `NOLINTBEGIN/NOLINTEND` blocks around macro definitions in `profiling.hpp` to suppress:
- `cppcoreguidelines-macro-usage` - Macros are intentional for zero-overhead
- `bugprone-macro-parentheses` - Tracy macros have specific syntax requirements

**Why:** Project policy requires clean clang-tidy, but profiling macros are an accepted exception similar to `error.hpp` pattern.

### Decision 4: Profiling Points Selection

Based on codebase analysis, instrument these critical paths:

**Main Loop (`src/app/main.cpp`):**
- Frame boundary: `GOGGLES_PROFILE_FRAME("Main")` at loop start
- Event processing scope
- Capture poll scope
- Render/clear call scope

**Vulkan Backend (`src/render/backend/vulkan_backend.cpp`):**
- `render_frame()` - Full frame render
- `acquire_next_image()` - Swapchain acquire
- `record_render_commands()` - Command buffer recording
- `submit_and_present()` - Queue submit and present
- `import_dmabuf()` - DMA-BUF import (potentially expensive)
- `create_swapchain()` - Swapchain creation (infrequent but heavy)
- `recreate_swapchain()` - Resize handling

**Filter Chain (`src/render/chain/filter_chain.cpp`):**
- `record()` - Full chain execution
- `ensure_framebuffers()` - Framebuffer management
- `load_preset()` - Preset loading (startup only)

**Filter Pass (`src/render/chain/filter_pass.cpp`):**
- `record()` - Per-pass execution with pass index tag
- `init()` - Pass initialization
- `create_pipeline()` - Pipeline creation (startup only)
- `update_descriptor()` - Descriptor updates

**Shader Runtime (`src/render/shader/shader_runtime.cpp`):**
- `compile_shader()` - Overall compilation
- `compile_retroarch_shader()` - RetroArch shader path
- `compile_glsl_with_reflection()` - GLSL compilation
- `load_cached_spirv()` / `save_cached_spirv()` - Cache I/O

**Capture Receiver (`src/capture/capture_receiver.cpp`):**
- `poll_frame()` - Frame reception
- `init()` - Initialization

**Capture Layer (`src/capture/vk_layer/vk_capture.cpp`):**
- `on_present()` - Present interception (minimal instrumentation)
- `capture_frame()` - Frame capture
- `record_copy_commands()` - Copy command recording

**Note:** Capture layer instrumentation must be minimal due to performance constraints (Project Policy B.4).

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Tracy overhead in hot paths | Compile-time elimination when disabled; minimal zones in vk_layer |
| Macro proliferation | Limited to profiling.hpp; follows established error.hpp pattern |
| Version compatibility | Pin Tracy version in CPM; test with CI |
| Binary size increase | Tracy client adds ~100KB; acceptable for debug/profile builds |

## Migration Plan

1. Add Tracy dependency (disabled by default)
2. Create profiling.hpp with macro definitions
3. Add instrumentation to identified code paths
4. Update build presets with `profile` preset
5. Document usage in README or docs/

**Rollback:** Remove `ENABLE_PROFILING` option usage; profiling code compiles to nothing.

## Open Questions

1. Should GPU zones (`TracyVkZone`) be included in initial implementation or deferred?
   - **Recommendation:** Defer to follow-up change. Requires command buffer integration.

2. Should capture layer use Tracy or remain uninstrumented for minimal overhead?
   - **Recommendation:** Minimal instrumentation only (`on_present`, `capture_frame`). Skip hot paths per policy B.4.
