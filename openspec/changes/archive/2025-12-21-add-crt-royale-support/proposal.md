# Change: Add RetroArch CRT Royale Support

## Why

We need to support the RetroArch `crt-royale` preset. It relies on multi-pass aliases, external LUT textures, per-texture sampler state, and a large runtime-parameter UBO. The current filter chain only binds a single source texture and ignores preset textures, aliases, and UBO parameters, so crt-royale cannot render correctly.

## Investigation Summary

- Preset textures and aliases are parsed but never used in runtime execution (`src/render/chain/preset_parser.hpp`, `src/render/chain/preset_parser.cpp`, `src/render/chain/filter_chain.cpp`).
- `FilterPass` binds every reflected sampler to the same `source_view` with a single clamp/no-mip sampler (`src/render/chain/filter_pass.cpp`).
- Push constants only cover standard semantics; aliased `ALIASSize` inputs (e.g., `VERTICAL_SCANLINESSize`) are not populated (`src/render/chain/filter_pass.cpp`, `src/render/chain/semantic_binder.hpp`).
- UBO parameters are never populated beyond MVP, while crt-royale defines a large `global` UBO (`src/render/chain/filter_pass.cpp`, `src/render/shader/slang_reflect.cpp`).
- No existing image/PNG loader in `src/` for LUT textures (grep search for stb/image loaders returned none).

## What Changes

- Load and bind preset-defined textures (mask LUTs and other external assets) by name.
- Honor per-texture sampling flags (`*_linear`, `*_mipmap`, `*_wrap_mode`).
- Route pass outputs via preset `aliasN` names and bind alias textures in later passes.
- Provide `ALIASSize` push constants for aliased inputs.
- Populate parameter values into UBO members and push constants by name, applying preset parameter overrides.

## Impact

- Affected specs: `render-pipeline` (ADDED requirements for preset textures, alias routing, and parameter binding).
- Affected code:
  - `src/render/chain/filter_chain.*` (load textures, alias map, parameter overrides)
  - `src/render/chain/filter_pass.*` (per-binding sampler, texture routing, size push constants, UBO parameter writes)
  - `src/render/chain/preset_parser.*` (wrap_mode parsing)
  - `src/render/shader/slang_reflect.*` (UBO member metadata already available; used for name-to-offset mapping)
- Dependencies: add a PNG loader (likely `stb_image` or equivalent) for LUT assets.
- Verification: add/extend tests to confirm preset texture parsing + alias size binding + UBO parameter mapping.
