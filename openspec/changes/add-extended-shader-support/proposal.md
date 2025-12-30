# Change: Add Extended RetroArch Shader Support

## Why

Current shader support covers basic CRT features (LUT textures, aliases, parameters). To support Mega-Bezel, NTSC compositing, and motion effects, we need additional RetroArch semantics.

## What's Added

### New Semantics
- **OriginalHistory[0-6]**: Previous frame textures for afterglow/motion effects
- **PassOutput#/PassFeedback#**: Inter-pass texture references by index
- **frame_count_mod**: Per-pass periodic frame counting (for NTSC alternating)
- **Rotation**: Display rotation push constant (0-3 for 0/90/180/270°)

### Parser Extensions
- **#reference directive**: Preset inclusion for modular preset structure
- Recursive reference loading with depth limit (max 8)
- Path cycle detection to prevent infinite loops

### Frame History
- Ring buffer for previous frames (MAX_HISTORY = 7)
- Lazy allocation based on shader requirements
- Auto-detect required depth from sampler names

## Compatibility Results

**Total: 1702/1906 presets compile (89%)**

| Status | Categories |
|--------|------------|
| ✅ 100% | anti-aliasing, blurs, border, cel, deblur, deinterlacing, denoisers, dithering, downsample, edge-smoothing, film, gpu, handheld, hdr, interpolation, misc, motionblur, ntsc, pal, reshade, scanlines, sharpen, vhs, warp |
| ⚠️ Partial | bezel (757/958), crt (115/117), pixel-art-scaling (22/23) |

### Known Limitations
- Mega Bezel GLASS/ADV presets: Missing `shaders` count due to complex #reference chains
- Some bezel presets require parameters not yet exposed via GUI

## Impact

- Affected code:
  - `src/render/chain/filter_chain.*` - frame history, feedback textures
  - `src/render/chain/filter_pass.*` - new semantic bindings
  - `src/render/chain/semantic_binder.*` - OriginalHistory, PassOutput, PassFeedback
  - `src/render/chain/preset_parser.*` - frame_count_mod, #reference
  - `src/render/chain/frame_history.*` - new ring buffer implementation
  - `src/render/chain/framebuffer.*` - waitIdle on shutdown
- Breaking changes: none
