# Filter Chain Integration Workflow

This guide is for contributors wiring `src/render/chain` into the runtime render loop.
It focuses on practical flow, not API contract details.

## Primary integration points

- Render-loop call sites: `src/render/backend/vulkan_backend.cpp`
- Chain orchestration: `src/render/chain/filter_chain.cpp`
- Pass ordering/sizing logic: `src/render/chain/filter_chain_core.cpp`
- Pass implementations: `src/render/chain/filter_pass.cpp`, `src/render/chain/output_pass.cpp`, `src/render/chain/downsample_pass.cpp`

## Golden path

1. Build or reload chain resources from a preset.
2. Keep the active chain stable while rebuild work runs.
3. Swap to rebuilt resources only when the new chain is fully ready.
4. Record one frame in fixed order: prechain -> effect -> output.
5. Advance history/feedback resources.
6. Destroy old resources after successful swap.

## Frame loop checklist

- Command buffer is valid and in recording state before chain record calls.
- Source and target views match the frame being processed.
- Stage order stays fixed (`record_prechain` -> effect pass loop -> `record_postchain`).
- Hot path stays lean: no avoidable file I/O, no avoidable allocations, no blocking waits.
- On record failure, treat the command buffer as unsafe and re-record cleanly.

## Preset load and rebuild flow

- Keep rebuild work off the hot path.
- Build replacement resources separately from the active chain.
- Never partially mutate the active graph in-place.
- Only publish new chain state after full validation succeeds.
- On failure, keep the previous known-good chain active.

## Resize flow

- Treat resize as a rebuild trigger, not a quick patch.
- Recompute pass output sizes through `filter_chain_core` sizing logic.
- Recreate graph-owned framebuffers/history resources as needed.
- Keep source/target extent assumptions aligned with backend render targets.

## Controls flow

- `control_id` is the stable mutation key while a preset layout is unchanged.
- Preserve deterministic control listing order: prechain, effect, postchain.
- Clamp invalid ranges consistently through shared control helpers.
- Keep control list/reset/set behavior in sync with `filter_controls.*` tests.

## Failure handling expectations

- Invalid input paths fail early and keep current runtime state usable.
- Rebuild failures do not corrupt the active chain.
- Failed swap paths clean up pending resources and reset swap flags.
- Error logging stays actionable and low-noise at subsystem boundaries.

## Quick debug checklist

- Wrong output stage? Check pass order and stage mask handling first.
- Flicker/history artifacts? Check feedback and `OriginalHistory#` bindings.
- Resize-only regressions? Check framebuffer lifetime and extent propagation.
- Missing control updates? Check `control_id` mapping and clamp/reset flow.

## Verification

- Unit/integration loop: `ctest --preset test -R "goggles_unit_tests" --output-on-failure`
- Full local gate: `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality`
- Runtime smoke: `pixi run start -p debug -- vkcube --wsi xcb`
