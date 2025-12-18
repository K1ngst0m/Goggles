# Change: Add Aspect Ratio Display Modes

## Why

Currently, Goggles stretches the captured image to fill the entire window, ignoring the original aspect ratio. This distorts the image when the window aspect ratio differs from the source. Users need control over how the captured frame is scaled and positioned within the output window.

## What Changes

- Add three display modes:
  - **Fit**: Scale image to fit entirely within the window (letterbox/pillarbox as needed)
  - **Fill**: Scale image to fill the window completely (crop edges as needed)
  - **Stretch**: Current behavior - force image to match window dimensions exactly
- Add `scale_mode` configuration option to `[render]` section in `goggles.toml`
- Calculate effective viewport dimensions based on scale mode for filter chain integration
- Modify `OutputPass` to calculate viewport/scissor based on selected mode

## Filter Chain Integration

This change specifically affects the **final OutputPass** rendering, which occurs after the filter chain. The interaction with `scale_type = viewport` in RetroArch presets requires careful handling:

### FinalViewportSize Semantic

When a shader pass uses `scale_type = viewport`, it renders at `FinalViewportSize`. This semantic should represent the **effective content area**, not the raw swapchain size:

| Scale Mode | Swapchain | Source Aspect | FinalViewportSize |
|------------|-----------|---------------|-------------------|
| Stretch | 1920x1080 | 4:3 | 1920x1080 (full swapchain) |
| Fit | 1920x1080 | 4:3 | 1440x1080 (letterboxed area) |
| Fill | 1920x1080 | 4:3 | 1920x1440 (cropped, viewport exceeds swapchain) |

### Design Decision

For initial implementation:
- **Fit mode**: `FinalViewportSize` = effective letterboxed area within swapchain
- **Fill mode**: `FinalViewportSize` = scaled area (may exceed swapchain bounds, scissor clips)
- **Stretch mode**: `FinalViewportSize` = swapchain size (current behavior)

This ensures shaders using `scale_type = viewport` render at the correct resolution for the selected display mode.

## Impact

- Affected specs: `render-pipeline`
- Affected code:
  - `src/util/config.hpp` - add `ScaleMode` enum and config field
  - `src/util/config.cpp` - parse new config option
  - `src/render/chain/filter_chain.*` - calculate FinalViewportSize based on scale mode
  - `src/render/chain/output_pass.cpp` - implement viewport/scissor positioning
  - `config/goggles.toml` - add new setting with documentation
