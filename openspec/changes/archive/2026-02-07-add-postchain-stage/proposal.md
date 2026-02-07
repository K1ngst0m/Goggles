# Change: Introduce post-chain stage with generic pass infrastructure

## Why

The current `OutputPass` is a single, hardcoded pass that renders the final output to the swapchain using blit shaders. This design limits extensibility for post-RetroArch effects like scanline overlays, color grading, or format conversion that should run after the RetroArch shader chain but before final presentation.

A post-chain stage, symmetric to the recently-implemented pre-chain, enables future post-processing while maintaining the existing blit-to-swapchain behavior as one pass in a vector.

## What Changes

### Post-Chain Infrastructure (generic, extensible)

- Introduce post-chain as a **vector of passes** (`m_postchain_passes`) in `FilterChain`, analogous to `m_prechain_passes`
- Add corresponding **vector of framebuffers** (`m_postchain_framebuffers`) for intermediate results
- Post-chain executes after RetroArch passes; its final output goes to the swapchain
- The existing `OutputPass` becomes the **last** pass in the post-chain vector (always present)
- Post-chain passes can be added/configured independently (future: scanline overlay, HDR tonemapping, etc.)

### OutputPass Refactoring

- `OutputPass` remains unchanged internally (still uses blit.vert/frag.slang)
- `OutputPass` is added to `m_postchain_passes` during `FilterChain::create()`
- Remove `m_output_pass` single pointer, use `m_postchain_passes.back()` for final output
- `record_postchain()` iterates all post-chain passes, with the last one rendering to swapchain

### Recording Flow

Current:
```
pre-chain passes -> RetroArch passes -> m_output_pass->record()
```

After:
```
pre-chain passes -> RetroArch passes -> post-chain passes (last = output)
```

## Impact

- Affected specs: render-pipeline
- Affected code:
  - `src/render/chain/filter_chain.hpp` - add post-chain vectors, remove `m_output_pass` member
  - `src/render/chain/filter_chain.cpp` - implement `record_postchain()`, refactor initialization
