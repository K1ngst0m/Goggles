# Tasks

## 1. Configuration Support
- [ ] 1.1 Add `ScaleMode` enum to `config.hpp` with values `Fit`, `Fill`, `Stretch`
- [ ] 1.2 Add `scale_mode` field to `Config::Render` struct (default: `Stretch`)
- [ ] 1.3 Update `config.cpp` to parse `scale_mode` from TOML
- [ ] 1.4 Add `scale_mode` setting to `config/goggles.toml` with documentation

## 2. Viewport Calculation Utility
- [ ] 2.1 Create `ScaledViewport` struct (offset_x, offset_y, width, height)
- [ ] 2.2 Implement `calculate_viewport(source_extent, target_extent, scale_mode)` helper
- [ ] 2.3 Implement Fit mode calculation (scale to fit, center)
- [ ] 2.4 Implement Fill mode calculation (scale to fill, center, may exceed bounds)
- [ ] 2.5 Implement Stretch mode calculation (use target_extent directly)

## 3. Filter Chain Integration
- [ ] 3.1 Pass `ScaleMode` to FilterChain (via config or explicit parameter)
- [ ] 3.2 Calculate `FinalViewportSize` based on scale mode and source aspect ratio
- [ ] 3.3 Update SemanticBinder to use calculated FinalViewportSize
- [ ] 3.4 Ensure passes with `scale_type = viewport` render at correct resolution

## 4. OutputPass Viewport Rendering
- [ ] 4.1 Add scale mode parameter to `OutputPass::record()` or `PassContext`
- [ ] 4.2 Use `calculate_viewport()` to determine actual viewport position/size
- [ ] 4.3 Set viewport to calculated values (may be offset from origin for Fit mode)
- [ ] 4.4 Set scissor to swapchain bounds (clips Fill mode overflow)
- [ ] 4.5 Ensure clear color fills letterbox/pillarbox areas (already black via clear)

## 5. Integration
- [ ] 5.1 Wire config scale_mode through to filter chain and output pass
- [ ] 5.2 Handle window resize: recalculate viewport on swapchain recreation

## 6. Testing
- [ ] 6.1 Manual test: Fit mode with 4:3 source in 16:9 window (letterbox)
- [ ] 6.2 Manual test: Fit mode with 16:9 source in 4:3 window (pillarbox)
- [ ] 6.3 Manual test: Fill mode crops correctly
- [ ] 6.4 Manual test: Stretch mode maintains current behavior
- [ ] 6.5 Manual test: scale_type=viewport shader renders at correct FinalViewportSize
- [ ] 6.6 Manual test: Config parsing for all three modes
