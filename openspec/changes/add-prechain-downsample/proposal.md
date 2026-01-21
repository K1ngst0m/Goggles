# Change: Introduce pre-chain stage with area downsampling pass

## Why

The `--app-width` / `--app-height` CLI options currently only work with WSI proxy mode. Users need a way to control the source resolution fed into the RetroArch filter chain regardless of capture mode. A pre-chain stage enables resolution control and other preprocessing for shader effects that benefit from modified input (CRT simulation, pixel art upscalers) without requiring WSI proxy overhead.

## What Changes

### Pre-Chain Infrastructure (generic, extensible)

- Introduce pre-chain as a **vector of passes** (`m_prechain_passes`) in `FilterChain`, analogous to `m_passes` for RetroArch
- Add corresponding **vector of framebuffers** (`m_prechain_framebuffers`) for intermediate results
- Pre-chain executes before RetroArch passes; its final output becomes `original_view` for the RetroArch chain
- Pre-chain passes can be added/configured independently (future: sharpening, color correction, etc.)

### Downsample Pass (first pre-chain pass)

- Add `downsample.frag.slang` shader in `shaders/internal/` using area filtering
- Create `DownsamplePass` class implementing the pass interface
- Add `DownsamplePass` to pre-chain when `--app-width`/`--app-height` are specified
- Support single-dimension specification with aspect-ratio preservation

### CLI Semantics

- Change `--app-width`/`--app-height` semantics: set source resolution for filter chain input (all capture modes)
- Support single-dimension: other dimension calculated from captured frame's aspect ratio
- Store configured resolution in `Config::Render`

## Impact

- Affected specs: render-pipeline
- Affected code:
  - `shaders/internal/downsample.frag.slang` (new)
  - `src/render/chain/downsample_pass.hpp/cpp` (new)
  - `src/render/chain/filter_chain.hpp` - add pre-chain vectors
  - `src/render/chain/filter_chain.cpp` - implement generic pre-chain recording
  - `src/app/cli.cpp` - update option descriptions
  - `src/util/config.hpp` - add source resolution fields
