## 1. Preset Parser Extensions

- [x] 1.1 Add `#reference` directive detection before INI parsing
- [x] 1.2 Implement recursive reference loading with depth limit (max 8)
- [x] 1.3 Add path cycle detection to prevent infinite loops
- [x] 1.4 Parse `frame_count_modN` per-pass into PassConfig
- [x] 1.5 Resolve relative paths for referenced presets

## 2. Frame History Ring Buffer

- [x] 2.1 Add FrameHistory struct with circular buffer (MAX_HISTORY = 7)
- [x] 2.2 Integrate FrameHistory into FilterChain
- [x] 2.3 Push Original texture to history each frame after processing
- [x] 2.4 Auto-detect required history depth from shader sampler names
- [x] 2.5 Lazy allocate history textures based on detected depth

## 3. Semantic Binding Extensions

- [x] 3.1 Bind OriginalHistory[0-6] textures by sampler name pattern
- [x] 3.2 Add OriginalHistorySize[0-6] via alias_size binding
- [x] 3.3 Apply frame_count_mod to FrameCount in FilterPass
- [x] 3.4 Add Rotation push constant (0-3 for 0/90/180/270 degrees)

## 4. Unit Tests

- [x] 4.1 Test #reference parsing with nested references
- [x] 4.2 Test #reference depth limit enforcement
- [x] 4.3 Test frame_count_mod parsing
- [x] 4.4 Test OriginalHistory sampler name pattern matching

## 5. Feedback Texture Support

- [x] 5.1 Detect *Feedback texture patterns from shader bindings
- [x] 5.2 Create feedback framebuffers for passes with aliases referenced as feedback
- [x] 5.3 Bind AliasFeedback textures during pass rendering
- [x] 5.4 Copy current framebuffer to feedback at end of frame
- [x] 5.5 Add AliasFeedbackSize semantics

## 6. Integration Tests

- [x] 6.1 Load and parse MBZ__5__POTATO preset (14 passes)
- [x] 6.2 Load and parse MBZ__3__STD preset (30 passes)
- [x] 6.3 Run ntsc-adaptive with frame_count_mod = 2
- [ ] 6.4 Visual verification of hsm-afterglow phosphor effect

## 7. Spec Compliance (SHADER_SPEC.md)

- [x] 7.1 Add PassOutput# texture bindings by pass number
- [x] 7.2 Add PassOutput#Size UBO members
- [x] 7.3 Add PassFeedback# bindings by pass number
- [x] 7.4 Add PassFeedback#Size UBO members
- [x] 7.5 Bind OriginalHistory0 = Original (spec requirement)
- [x] 7.6 Fix binding order (clear before set)

## 8. Pending Issues (Mega Bezel)

- [ ] 8.1 Visual verification of Mega Bezel screen content
- [ ] 8.2 Verify InfoCachePass receives correct DerezedPassSize