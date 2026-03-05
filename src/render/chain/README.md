# Filter Chain Maintainer Guide

This directory owns Goggles' multi-pass shader pipeline.
Use this file as the entry point before changing chain behavior.

## What this module owns

- Preset parsing for `.slangp` pipelines.
- Pass-graph build and rebuild.
- Per-frame recording for prechain, effect passes, and output pass.
- Frame history and feedback surfaces (`OriginalHistory#`, `PassFeedback#`).
- Control discovery/mutation plumbing used by UI and runtime boundaries.

## Runtime flow (mental model)

1. Load preset and build pass graph.
2. Allocate/update graph-owned framebuffers.
3. Per frame, record in fixed order: prechain -> effect -> output.
4. Advance history and feedback resources for next frame.
5. Rebuild when preset or size policy changes.

## Where to edit

| Task | Primary files | Also check |
|---|---|---|
| Pipeline orchestration | `filter_chain.cpp`, `filter_chain.hpp` | `filter_chain_core.cpp` |
| Pass execution order and sizing | `filter_chain_core.cpp`, `filter_chain_core.hpp` | `output_pass.cpp` |
| Single pass behavior | `filter_pass.cpp`, `filter_pass.hpp` | `semantic_binder.hpp` |
| Output stage | `output_pass.cpp`, `output_pass.hpp` | `framebuffer.*` |
| Pre-filter/downsample path | `downsample_pass.cpp`, `downsample_pass.hpp` | `frame_history.*` |
| Preset parsing | `preset_parser.cpp`, `preset_parser.hpp` | tests under `tests/render/` |
| Shader semantic binding | `semantic_binder.hpp` | `tests/render/test_filter_chain.cpp` |
| Control metadata and IDs | `filter_controls.cpp`, `filter_controls.hpp` | `tests/render/test_filter_controls.cpp` |

## Invariants you must preserve

- Stage order stays fixed: prechain -> effect -> output.
- Semantic names stay stable: `Source`, `OriginalHistory#`, `PassOutput#`, `PassFeedback#`.
- Rebuild work stays coordinated through `util::JobSystem`; no ad-hoc thread ownership in chain code.
- Framebuffer/history lifetime stays graph-scoped; avoid cross-graph leakage.
- Keep hot path bounded: no avoidable blocking I/O, no avoidable allocations in per-frame record paths.

## Practical change playbooks

### Change preset behavior

1. Update parser/model (`preset_parser.*`).
2. Update pass-graph build logic (`filter_chain_core.*`).
3. Verify semantic bindings still resolve as expected.
4. Run render chain unit tests.

### Change pass execution or resources

1. Update pass implementation (`filter_pass.*`, `output_pass.*`, or `downsample_pass.*`).
2. Verify stage order and resource transitions still match contracts.
3. Validate feedback/history continuity.
4. Run render tests and one real render smoke run.

### Change control behavior

1. Update `filter_controls.*` and chain integration points.
2. Keep `control_id` semantics stable for active presets.
3. Verify clamp/reset/list behavior in tests.

## Verification checklist

- Fast loop: `ctest --preset test -R "goggles_unit_tests" --output-on-failure`
- CI-parity gate: `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality`
- Manual smoke: `pixi run start -p debug -- vkcube --wsi xcb`

## Related docs

- Workflow guide: `docs/integration_guide.md`
- Maintainer contracts: `docs/spec.md`
