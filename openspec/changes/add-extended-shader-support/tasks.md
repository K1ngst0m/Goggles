## 1. Preset Parser Extensions

- [x] 1.1 Add `#reference` directive detection before INI parsing
- [x] 1.2 Implement recursive reference loading with depth limit (max 8)
- [x] 1.3 Add path cycle detection to prevent infinite loops
- [x] 1.4 Parse `frame_count_modN` per-pass into PassConfig
- [x] 1.5 Resolve relative paths for referenced presets

## 2. Frame History Ring Buffer

- [x] 2.1 Add FrameHistory struct with circular buffer (MAX_HISTORY = 7)
- [x] 2.2 Integrate FrameHistory into FilterChain
- [ ] 2.3 Push Original texture to history each frame after processing
- [ ] 2.4 Auto-detect required history depth from shader sampler names
- [ ] 2.5 Lazy allocate history textures based on detected depth

## 3. Semantic Binding Extensions

- [x] 3.1 Bind OriginalHistory[0-6] textures by sampler name pattern
- [x] 3.2 Add OriginalHistorySize[0-6] via alias_size binding
- [x] 3.3 Apply frame_count_mod to FrameCount in FilterPass
- [ ] 3.4 Add Rotation push constant (0-3 for 0/90/180/270 degrees)

## 4. Unit Tests

- [ ] 4.1 Test #reference parsing with nested references
- [ ] 4.2 Test #reference depth limit enforcement
- [ ] 4.3 Test frame_count_mod parsing
- [ ] 4.4 Test OriginalHistory sampler name pattern matching

## 5. Integration Tests

- [ ] 5.1 Load and parse MBZ__5__POTATO preset (12 passes)
- [ ] 5.2 Load and parse MBZ__3__STD preset (30 passes)
- [ ] 5.3 Run ntsc-adaptive with frame_count_mod = 2
- [ ] 5.4 Visual verification of hsm-afterglow phosphor effect