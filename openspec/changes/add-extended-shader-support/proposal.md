# Change: Add Extended RetroArch Shader Support

## Why

Current shader support covers CRT-royale features (LUT textures, aliases, parameters). To support Mega-Bezel and other complex presets, we need additional RetroArch semantics.

## Target Shaders

### Mega-Bezel (660 presets)
- **MBZ__5__POTATO** - 12 passes, minimal effects
- **MBZ__3__STD** - 30 passes, standard bezel + CRT
- **MBZ__0__SMOOTH-ADV** - 46 passes, full effects + NTSC

Requirements: 20+ LUT textures, frame_count_mod, mipmap_input, 30+ aliases

### NTSC Compositing
- **ntsc-adaptive** - 4 passes, composite video simulation
- **ntsc-svideo** - 3 passes, S-Video simulation

Requirements: frame_count_mod = 2

### Motion Effects
- **motion-interpolation** - frame interpolation
- **hsm-afterglow** - CRT phosphor afterglow

Requirements: OriginalHistory[0-1]

## What Changes

- **OriginalHistory[0-6]**: Previous frame textures for afterglow/motion
- **frame_count_mod**: Periodic frame counting for NTSC alternating
- **#reference directive**: Preset inclusion for Mega-Bezel modular structure
- **Rotation semantic**: Display rotation for bezels

## Impact

- Affected specs: `render-pipeline`
- Affected code:
  - `src/render/chain/filter_chain.*` - frame history ring buffer
  - `src/render/chain/filter_pass.*` - new semantic bindings
  - `src/render/chain/semantic_binder.hpp` - OriginalHistory, Rotation
  - `src/render/chain/preset_parser.*` - frame_count_mod, #reference
- Dependencies: none
- Breaking changes: none