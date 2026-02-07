## 1. Create Area Downsample Shader

- [x] 1.1 Create `shaders/internal/downsample.frag.slang` with area filter algorithm
- [x] 1.2 Add push constant for source/target dimensions to calculate sample weights
- [x] 1.3 Verify shader compiles with Slang in HLSL mode

## 2. Create DownsamplePass Class

- [x] 2.1 Create `downsample_pass.hpp/cpp` with pass interface
- [x] 2.2 Implement pipeline creation, descriptor sets, push constants
- [x] 2.3 Implement `record()` method for command buffer recording

## 3. Implement Generic Pre-Chain Infrastructure

- [x] 3.1 Add `m_prechain_passes` vector to `FilterChain` (not single `m_downsample_pass`)
- [x] 3.2 Add `m_prechain_framebuffers` vector to `FilterChain` (not single `m_downsample_framebuffer`)
- [x] 3.3 Implement `add_prechain_pass()` method to append passes to pre-chain
- [x] 3.4 Implement `record_prechain()` to iterate all pre-chain passes (not hardcoded downsample)
- [x] 3.5 Ensure pre-chain output becomes `original_view` for RetroArch chain

## 4. Integrate Downsample Pass into Pre-Chain

- [x] 4.1 Add `DownsamplePass` to pre-chain when source resolution configured
- [x] 4.2 Size final pre-chain framebuffer to configured resolution
- [x] 4.3 Implement lazy initialization for single-dimension (aspect-ratio calculation)

## 5. Update CLI and Config

- [x] 5.1 Update `--app-width`/`--app-height` help text to describe new semantics
- [x] 5.2 Add `source_width`/`source_height` to `Config::Render`
- [x] 5.3 Remove validation requiring both dimensions (allow single-dimension specification)
- [x] 5.4 Pass configured resolution through to `FilterChain::create()`

## 6. Integration and Testing

- [x] 6.1 Test downsampling with high-res capture (e.g., 1920x1080 -> 640x480)
- [x] 6.2 Verify RetroArch shaders receive correct `OriginalSize` after downsampling
- [x] 6.3 Test with `--app-width 320 --app-height 240` to simulate retro resolution
- [x] 6.4 Verify existing behavior unchanged when options not provided
- [x] 6.5 Test single-dimension: `--app-width 640` with 1920x1080 source -> 640x360
- [x] 6.6 Test single-dimension: `--app-height 480` with 1920x1080 source -> 854x480
