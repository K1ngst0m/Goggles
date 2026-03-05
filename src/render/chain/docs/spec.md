# Filter Chain Maintainer Contracts

This file captures the contracts that must stay true when changing `src/render/chain`.
If a change breaks one of these, treat it as a design-level regression.

## 1) Pipeline order contract

- Execution order is fixed: prechain -> effect -> output.
- New passes may extend a stage, but cannot reorder stages.
- Stage-order changes require coordinated updates to transitions, history, and tests.

## 2) Semantic name contract

- These names are part of the shader compatibility surface and must remain stable:
  - `Source`
  - `OriginalHistory#`
  - `PassOutput#`
  - `PassFeedback#`
- Any rename or meaning change must include migration handling and explicit review.

## 3) Graph ownership contract

- Framebuffers and history resources are owned by the current pass graph.
- Rebuild creates replacement resources; active resources are not partially mutated in-place.
- Resource cleanup happens after successful swap or during failure unwind.

## 4) Rebuild and swap contract

- Rebuild work is coordinated through `util::JobSystem` patterns.
- Do not introduce ad-hoc thread ownership in chain code.
- Swap publishes all-or-nothing state.
- Rebuild failure keeps the previously working chain active.

## 5) Record-path contract

- Per-frame record path is deterministic and bounded.
- Keep hot path free of avoidable file I/O, avoidable allocation spikes, and blocking waits.
- Record failures must not leave hidden partially-updated chain state.

## 6) Controls contract

- `control_id` remains the stable key for a preset layout.
- Full-list ordering remains deterministic: prechain, effect, postchain.
- Set-value behavior clamps through shared control logic.
- Reset one and reset all stay consistent with list output.

## 7) Preset parser contract

- Invalid preset input fails safely.
- Parser changes must keep error paths no-crash.
- Any parser behavior change that affects graph construction requires test updates in the same change.

## 8) Integration boundary contract

- Backend code controls frame loop timing and swap timing.
- Chain code owns pass-graph internals.
- Boundary updates should remain explicit in `vulkan_backend.cpp` and `filter_chain.cpp`.

## 9) Change-impact map

| You changed | Re-check immediately |
|---|---|
| `filter_chain_core.*` | stage order, sizing math, history/feedback continuity |
| `filter_pass.*` or `output_pass.*` | resource transitions, output correctness, stage boundaries |
| `framebuffer.*` or `frame_history.*` | lifetime, resize behavior, swap cleanup |
| `preset_parser.*` | parse failures, reference handling, graph build inputs |
| `semantic_binder.hpp` | semantic resolution, shader compatibility names |
| `filter_controls.*` | list order, clamp behavior, reset semantics |

## 10) Verification baseline

Run these before final review:

1. `ctest --preset test -R "goggles_unit_tests" --output-on-failure`
2. `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality`
3. `pixi run start -p debug -- vkcube --wsi xcb`

When touching parser/semantics/order logic, also inspect:

- `tests/render/test_filter_chain.cpp`
- `tests/render/test_preset_parser.cpp`
- `tests/render/test_semantic_binder.cpp`
- `tests/render/test_filter_controls.cpp`
- `tests/render/test_filter_boundary_contracts.cpp`

## 11) PR review guardrails

- If behavior changed, docs and tests should be updated in the same PR.
- If invariants are intentionally changed, call it out explicitly in the PR body.
- If you cannot prove a contract still holds, do not merge on assumption.
