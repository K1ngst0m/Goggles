# Change: Add Aspect Ratio Display Modes

## Status: In Progress

**32/33 tasks complete**

| Section | Progress | Notes |
|---------|----------|-------|
| Configuration Support | 6/6 | ScaleMode enum, integer_scale, TOML parsing |
| Viewport Calculation | 7/7 | All four modes implemented |
| Filter Chain Integration | 4/4 | SemanticBinder FinalViewportSize complete |
| OutputPass Rendering | 5/5 | Viewport/scissor with calculate_viewport() |
| Integration | 2/2 | Config wired through, resize handling |
| Manual Testing | 8/9 | Only scale_type=viewport test pending |

## Why

Currently, Goggles stretches the captured image to fill the entire window, ignoring the original aspect ratio. This distorts the image when the window aspect ratio differs from the source. Users need control over how the captured frame is scaled and positioned within the output window.

## What Changes

- Add four display modes via `scale_mode`:
  - **Fit**: Scale image to fit entirely within the window (letterbox/pillarbox as needed)
  - **Fill**: Scale image to fill the window completely (crop edges as needed)
  - **Stretch**: Current behavior - force image to match window dimensions exactly
  - **Integer**: Pixel-perfect integer scaling for retro content
- Add `integer_scale` setting (only applies when `scale_mode = "integer"`):
  - `0` or `"auto"`: Maximum integer multiplier that fits in the window
  - `1`: Original size (no scaling, "origin" mode)
  - `2-8`: Fixed integer multiplier
- Calculate effective viewport dimensions based on scale mode for filter chain integration
- Modify `OutputPass` to calculate viewport/scissor based on selected mode

## Configuration Design

```toml
[render]
# Display scaling mode
# Options: "fit", "fill", "stretch", "integer"
scale_mode = "stretch"

# Integer scaling multiplier (only used when scale_mode = "integer")
# 0 or "auto" = maximum that fits, 1 = original size, 2-8 = fixed multiplier
integer_scale = 0
```

The `integer_scale` setting is intentionally only active when `scale_mode = "integer"` to avoid user confusion (e.g., "why doesn't stretch fill my screen?").

## Filter Chain Integration

This change specifically affects the **final OutputPass** rendering, which occurs after the filter chain. The interaction with `scale_type = viewport` in RetroArch presets requires careful handling:

### FinalViewportSize Semantic

When a shader pass uses `scale_type = viewport`, it renders at `FinalViewportSize`. This semantic should represent the **effective content area**, not the raw swapchain size:

| Scale Mode | Swapchain | Source | FinalViewportSize |
|------------|-----------|--------|-------------------|
| Stretch | 1920x1080 | 640x480 | 1920x1080 (full swapchain) |
| Fit | 1920x1080 | 640x480 (4:3) | 1440x1080 (letterboxed area) |
| Fill | 1920x1080 | 640x480 (4:3) | 1920x1440 (cropped, exceeds bounds) |
| Integer (auto) | 1920x1080 | 640x480 | 1280x960 (2x, max that fits) |
| Integer (1) | 1920x1080 | 640x480 | 640x480 (original size) |
| Integer (3) | 1920x1080 | 640x480 | 1920x1440 (3x, may exceed/clip) |

### Design Decision

- **Fit/Fill modes**: Use source aspect ratio to calculate effective viewport
- **Integer mode**: Use raw source dimensions × scale factor (ignores aspect correction for pixel purity)
- **Stretch mode**: FinalViewportSize = swapchain size (current behavior)

This ensures shaders using `scale_type = viewport` render at the correct resolution for the selected display mode.

## Impact

- Affected specs: `render-pipeline`
- Affected code:
  - `src/util/config.hpp` - add `ScaleMode` enum, `integer_scale` field
  - `src/util/config.cpp` - parse new config options
  - `src/render/chain/filter_chain.*` - calculate FinalViewportSize based on scale mode
  - `src/render/chain/output_pass.cpp` - implement viewport/scissor positioning
  - `config/goggles.toml` - add new settings with documentation
