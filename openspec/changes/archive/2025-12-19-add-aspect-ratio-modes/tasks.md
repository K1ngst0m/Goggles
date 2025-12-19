# Tasks

## 1. Configuration Support
- [x] 1.1 Add `ScaleMode` enum to `config.hpp` with values `Fit`, `Fill`, `Stretch`, `Integer`
- [x] 1.2 Add `scale_mode` field to `Config::Render` struct (default: `Stretch`)
- [x] 1.3 Add `integer_scale` field to `Config::Render` struct (default: `0` = auto)
- [x] 1.4 Update `config.cpp` to parse `scale_mode` from TOML
- [x] 1.5 Update `config.cpp` to parse `integer_scale` from TOML (validate 0-8 range)
- [x] 1.6 Add settings to `config/goggles.toml` with documentation

## 2. Viewport Calculation Utility
- [x] 2.1 Create `ScaledViewport` struct (offset_x, offset_y, width, height)
- [x] 2.2 Implement `calculate_viewport(source_extent, target_extent, scale_mode, integer_scale)` helper
- [x] 2.3 Implement Fit mode: scale to fit preserving aspect ratio, center
- [x] 2.4 Implement Fill mode: scale to fill preserving aspect ratio, center, may exceed bounds
- [x] 2.5 Implement Stretch mode: use target_extent directly (no offset)
- [x] 2.6 Implement Integer mode with auto (0): find max integer scale that fits, center
- [x] 2.7 Implement Integer mode with fixed scale (1-8): multiply source by scale, center

## 3. Filter Chain Integration
- [x] 3.1 Pass `ScaleMode` and `integer_scale` to FilterChain
- [x] 3.2 Calculate `FinalViewportSize` based on scale mode and source dimensions
- [x] 3.3 Update SemanticBinder to use calculated FinalViewportSize
- [x] 3.4 Ensure passes with `scale_type = viewport` render at correct resolution

## 4. OutputPass Viewport Rendering
- [x] 4.1 Add scale mode parameters to `OutputPass::record()` or `PassContext`
- [x] 4.2 Use `calculate_viewport()` to determine actual viewport position/size
- [x] 4.3 Set viewport to calculated values (may be offset from origin)
- [x] 4.4 Set scissor to swapchain bounds (clips overflow in Fill/Integer modes)
- [x] 4.5 Ensure clear color fills letterbox/pillarbox areas (black via clear)

## 5. Integration
- [x] 5.1 Wire config settings through to filter chain and output pass
- [x] 5.2 Handle window resize: recalculate viewport on swapchain recreation

## 6. Testing
- [X] 6.1 Manual test: Fit mode with 4:3 source in 16:9 window (letterbox)
- [X] 6.2 Manual test: Fit mode with 16:9 source in 4:3 window (pillarbox)
- [X] 6.3 Manual test: Fill mode crops correctly
- [X] 6.4 Manual test: Stretch mode maintains current behavior
- [X] 6.5 Manual test: Integer mode auto (0) finds max scale that fits
- [X] 6.6 Manual test: Integer mode with scale=1 shows original size centered
- [X] 6.7 Manual test: Integer mode with scale=2 shows 2x centered
- [ ] 6.8 Manual test: scale_type=viewport shader renders at correct FinalViewportSize (deferred: no shader uses FinalViewportSize) 
- [X] 6.9 Manual test: Config parsing for all modes and integer_scale values
