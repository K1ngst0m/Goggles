## Context

`crt-royale` is a 12-pass RetroArch preset that depends on multiple external mask textures, alias-based pass routing (e.g., `VERTICAL_SCANLINES`), and a large UBO of runtime parameters. Goggles currently binds a single source texture to every sampler and only populates an MVP UBO, so crt-royale cannot render correctly.

## Goals / Non-Goals

- Goals:
  - Load and bind preset textures by name with correct sampler state.
  - Route alias outputs to later passes and provide alias size push constants.
  - Populate UBO parameters and apply preset overrides.
- Non-Goals:
  - OriginalHistory / PassFeedback semantics (deferred).
  - UI for tuning shader parameters at runtime (deferred).

## Decisions

- Decision: add a small texture-loading utility for PNG LUTs.
  - Rationale: crt-royale LUTs are PNG assets; a lightweight loader is sufficient.
  - Implementation: use `stb_image` (header-only) to decode, upload via staging buffer, and generate mipmaps when requested.

- Decision: bind textures by name rather than by index.
  - Rationale: RetroArch presets rely on named samplers (`Source`, `Original`, alias names, and LUT names).
  - Implementation: create a per-pass binding table `{binding -> texture source}` derived from reflection + preset alias/texture registry.

- Decision: sampler state is per-binding, not a single sampler per pass.
  - Rationale: crt-royale needs `repeat` wrap and mipmaps for LUTs, while source inputs often clamp.
  - Implementation: maintain a `SamplerKey` cache (filter, mipmap, wrap U/V/W) and assign per-binding samplers.

- Decision: use reflection member names to populate UBO parameters.
  - Rationale: crt-royale declares many parameters in `bind-shader-params.h` as UBO members.
  - Implementation: build a `{name -> offset}` map from `UniformMember` and write values each frame or once after creation.

## Risks / Trade-offs

- PNG decoding adds a new dependency and CPU work during preset load.
  - Mitigation: load once at preset initialization; cache textures for reuse.
- Per-binding samplers increase descriptor updates and resource count.
  - Mitigation: cache samplers and reuse across bindings with identical state.

## Migration Plan

1. Add loader + GPU upload for preset textures.
2. Extend preset parsing for wrap modes.
3. Add alias routing + size push constants.
4. Map parameter defaults/overrides into UBO members.

## Open Questions

- Should we support non-PNG LUT formats now (e.g., JPG, BMP), or only PNG?
- Do we want optional shared texture cache across presets?
