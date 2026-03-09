## Context

`DownsamplePass` already serves as the single prechain downsampling implementation and exposes a discrete `filter_type` runtime parameter for area and gaussian behavior. The current pass always binds a linear sampler, so adding true nearest-neighbor support requires an explicit design choice for how the runtime selects sampling behavior without breaking existing values, prechain control ordering, or same-frame parameter updates.

## Goals / Non-Goals

**Goals:**
- Extend the existing prechain `filter_type` path to support nearest-neighbor as a third runtime mode.
- Preserve current numeric/value compatibility for area and gaussian.
- Keep runtime switching pipeline-free so the next rendered frame reflects the selected mode.
- Keep the change local to `DownsamplePass`, the existing prechain control surfaces, and targeted tests/specs.

**Non-Goals:**
- Introducing a new prechain control identifier or a separate filter-mode subsystem.
- Changing RetroArch pass sampler semantics outside the prechain downsample path.
- Redesigning the shader-controls UI beyond what is required to expose the new discrete mode.
- Changing prechain stage ordering, semantic binding, or broader filter-chain ownership boundaries.

## Decisions

- Decision: Keep `filter_type` as the sole prechain downsample selector and extend its discrete range from `0..1` to `0..2`.
  - Rationale: this preserves the existing control ID, prechain descriptor flow, and persisted area/gaussian meanings while making nearest-neighbor additive.
  - Alternative considered: add a separate `filter_mode` parameter. Rejected because it creates an unnecessary second control path and complicates persistence/migration.

- Decision: Preserve numeric compatibility by keeping `0 = area`, `1 = gaussian`, and introducing `2 = nearest-neighbor`.
  - Rationale: existing persisted state and any prechain control snapshots continue to load without reinterpretation.
  - Alternative considered: remap defaults or reorder values. Rejected because it would create avoidable compatibility risk.

- Decision: Implement nearest-neighbor with an explicit nearest-sampling runtime path while leaving area and gaussian on the current linear-sampling path.
  - Rationale: true nearest-neighbor behavior cannot rely on the current always-linear sampler path. The pass should switch sampling behavior internally without creating a new pipeline family.
  - Alternative considered: approximate nearest-neighbor in the shader while keeping only the linear sampler. Rejected because it risks non-exact sampling semantics and unnecessary shader complexity.

- Decision: Keep verification focused on render-pipeline behavior, control metadata compatibility, and targeted filter-chain tests.
  - Rationale: this change is localized to prechain downsampling and should not broaden into unrelated UI or API redesign work.

## Risks / Trade-offs

- Control surfaces or tests may implicitly assume `filter_type.max_value == 1`.
  - Mitigation: update descriptor expectations and extend targeted prechain control tests.
- Nearest-neighbor intentionally increases aliasing relative to area filtering.
  - Mitigation: document the behavior in the render-pipeline spec as a distinct selectable mode rather than treating it as a regression.
- Adding a second sampler path increases pass-state complexity slightly.
  - Mitigation: keep sampler ownership inside `DownsamplePass` and avoid widening the change into shared pipeline abstractions.
- If nearest-neighbor support reveals any need to change async preset reload ordering, shutdown sequencing, or filter-boundary ownership, the current design is incomplete and those concerns require a separate artifact revision before code changes continue.

## Migration Plan

1. Update proposal/spec artifacts so the new filter-mode contract is self-contained.
2. Implement `DownsamplePass` runtime selection changes and shader behavior for nearest-neighbor.
3. Update control/persistence surfaces so the new mode round-trips without changing current values.
4. Run targeted unit coverage, then the full ASAN + quality verification contract.

## Open Questions

- None. The remaining work is implementation and verification, not product-scope discovery.
