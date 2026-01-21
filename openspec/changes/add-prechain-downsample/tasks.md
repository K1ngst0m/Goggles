## 1. Create Area Downsample Shader

- [ ] 1.1 Create `shaders/internal/downsample.frag.slang` with area filter algorithm
- [ ] 1.2 Add push constant for source/target dimensions to calculate sample weights
- [ ] 1.3 Verify shader compiles with Slang in HLSL mode

## 2. Extend FilterChain for Pre-Chain Passes

- [ ] 2.1 Add `m_prechain_passes` vector and `m_prechain_framebuffers` to `FilterChain`
- [ ] 2.2 Add `m_source_resolution` field to track configured source resolution (0,0 = disabled)
- [ ] 2.3 Create `init_prechain()` method to set up downsample pass when resolution is configured
- [ ] 2.4 Modify `FilterChain::create()` to accept optional source resolution parameter

## 3. Implement Pre-Chain Recording

- [ ] 3.1 Modify `FilterChain::record()` to run pre-chain passes before RetroArch passes
- [ ] 3.2 Use pre-chain output as `original_view` for RetroArch chain when pre-chain is active
- [ ] 3.3 Ensure `OriginalSize` semantic reflects pre-chain output (not raw capture) when active
- [ ] 3.4 Add image barriers for pre-chain framebuffer transitions

## 4. Update CLI and Config

- [ ] 4.1 Update `--app-width`/`--app-height` help text to describe new semantics
- [ ] 4.2 Add `source_width`/`source_height` to `Config::Render` (or appropriate section)
- [ ] 4.3 Remove WSI-proxy-only validation for `--app-width`/`--app-height`
- [ ] 4.4 Pass configured resolution through to `FilterChain::create()`

## 5. Integration and Testing

- [ ] 5.1 Test downsampling with high-res capture (e.g., 1920x1080 -> 640x480)
- [ ] 5.2 Verify RetroArch shaders receive correct `OriginalSize` after downsampling
- [ ] 5.3 Test with `--app-width 320 --app-height 240` to simulate retro resolution
- [ ] 5.4 Verify existing behavior unchanged when options not provided
