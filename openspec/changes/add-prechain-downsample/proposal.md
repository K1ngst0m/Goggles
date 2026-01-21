# Change: Introduce pre-chain stage with area downsampling pass

## Why

The `--app-width` / `--app-height` CLI options currently only work with WSI proxy mode. Users need a way to control the source resolution fed into the RetroArch filter chain regardless of capture mode. A pre-chain downsampling stage enables resolution control for shader effects that benefit from lower-res input (CRT simulation, pixel art upscalers) without requiring WSI proxy overhead.

## What Changes

- Add `downsample.frag.slang` shader in `shaders/internal/` using area filtering for high-quality downsampling
- Introduce `PreChain` infrastructure in `FilterChain` to run internal passes before RetroArch shader passes
- Add `DownsamplePass` as the first pre-chain pass when `--app-width`/`--app-height` are specified
- Reuse existing `FilterPass`/`Framebuffer` infrastructure for pre-chain passes
- Change `--app-width`/`--app-height` semantics: these options now set the source resolution for the filter chain input (applies to all capture modes, not just WSI proxy)
- Store configured source resolution in `Config::Capture` for use by `FilterChain`

## Impact

- Affected specs: render-pipeline
- Affected code:
  - `shaders/internal/downsample.frag.slang` (new)
  - `src/render/chain/filter_chain.hpp` - add pre-chain pass storage
  - `src/render/chain/filter_chain.cpp` - implement pre-chain recording
  - `src/app/cli.cpp` - update option descriptions
  - `src/util/config.hpp` - add source resolution fields
