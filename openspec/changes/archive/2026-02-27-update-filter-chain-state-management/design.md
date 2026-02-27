## Context

Filter-chain stage enablement is currently decided through multiple call sites and APIs:
- Application-side toggle resolution for global/per-surface/effect controls.
- Backend stage setters invoked separately for prechain and effect behavior.
- Filter-chain reinitialization and async chain swap paths that instantiate new chain objects.

This split ownership makes stage state vulnerable to drift. During async preset reload or swapchain-triggered chain recreation, new `FilterChain` instances can run with default stage settings before runtime toggles are re-applied. The current prechain lifecycle also allows stale downsampling resources to survive toggle/resize transitions.

The startup path has an additional race between three systems:
- Surface defaults are inferred while capture source can still be transitioning.
- Per-surface maximize/restore requests are issued before policy stabilization.
- Prechain UI initialization can latch target extent from the first observed frame.

This race explains observed launch variance (`500x500`, `1920x1080`, and mixed states) across identical runs.

Constraints:
- Keep current three user controls and semantics.
- Preserve compositor-style resize behavior when global/per-surface filtering is disabled.
- Preserve mandatory presentation via postchain output blit.
- Keep implementation aligned with current app/backend module boundaries.
- Keep one proposal/change-id; do not split startup determinism into a separate OpenSpec change.

## Goals / Non-Goals

**Goals:**
- Centralize effective stage resolution from the three GUI controls into one policy.
- Apply policy atomically to runtime stages (prechain/effect) through one backend API.
- Ensure policy persists across filter-chain recreation and async chain swaps.
- Remove prechain downsampling state ambiguity across toggle and resize transitions.
- Clarify and codify that postchain output blit remains active even when global filtering is disabled.

**Non-Goals:**
- Changing UI layout or adding new end-user toggles.
- Replacing the existing async shader/preset load architecture.
- Reworking output-pass rendering into a separate non-filter-chain renderer.

## Decisions

- Decision: Introduce a resolved runtime policy object computed once per frame in `Application`.
  - Policy fields: global enabled, per-surface enabled, effect checkbox enabled, effective prechain enabled, effective effect enabled.
  - Rationale: a single resolver eliminates duplicated precedence logic and conflicting call order.
  - Alternative considered: keep separate boolean setters and improve call ordering.
    - Rejected because ordering fixes are fragile under future code paths.

- Decision: Replace split backend stage toggles with one `set_filter_chain_policy(...)` backend entry point.
  - Rationale: backend applies prechain/effect updates in one operation and owns current runtime policy.
  - Alternative considered: keep `set_prechain_enabled` and `set_shader_enabled` but enforce pairing.
    - Rejected because API design still permits drift and incomplete application.

- Decision: Persist the active policy in backend state and reapply it on any chain object replacement (`init_filter_chain`, async swap).
  - Rationale: guarantees newly created chains inherit active runtime gating before first render.
  - Alternative considered: defer reapply to next frame from app loop.
    - Rejected because it allows one-frame default behavior leakage.

- Decision: Keep postchain output blit always active; global/per-surface OFF bypasses prechain and effect stage only.
  - Rationale: output pass is the canonical presentation path and should remain stable regardless of filter effect routing.
  - Alternative considered: fully bypass all stages including postchain.
    - Rejected due to added renderer complexity and behavior divergence from current architecture.

- Decision: Separate requested prechain resolution from resolved runtime prechain extent and rebuild prechain resources on basis changes.
  - Rationale: prevents stale downsample targets and avoids mutating requested aspect-preserve semantics.
  - Alternative considered: continue mutating one shared prechain resolution value.
    - Rejected because it obscures source-of-truth and causes unstable resize/toggle outcomes.

- Decision: Use the frame's actual source path when updating history inputs.
  - Rationale: history should reflect executed stage output, not stale prechain resources.

- Decision: Resolve and persist a stable session capture mode before per-surface defaulting and resize orchestration.
  - Rationale: removes dependence on transient `has_frame()` timing during startup.
  - Alternative considered: infer defaults from live frame availability each sync tick.
    - Rejected because arrival order causes nondeterministic defaults.

- Decision: In direct Vulkan capture sessions, default prechain target initialization uses viewer swapchain extent.
  - Rationale: user-selected behavior `1` prefers stable desktop-size startup behavior.
  - Alternative considered: default to first stable app/source extent.
    - Rejected because it preserves startup race outcomes and native-size drift.

- Decision: Apply compositor maximize/restore requests only on effective policy transitions (or surface topology changes), not every periodic sync pass.
  - Rationale: prevents resize oscillation and contradictory requests while startup state settles.

## Risks / Trade-offs

- [Policy migration mistakes in call sites] -> Mitigation: remove/limit old setter paths and route all updates through one API.
- [One-frame regressions during chain swap] -> Mitigation: apply persisted policy immediately after new chain is installed.
- [Behavior confusion around "filter chain off" wording] -> Mitigation: specs explicitly state postchain output remains active.
- [Prechain rebuild frequency increases] -> Mitigation: gate rebuilds by precise resolution/basis changes only.
- [Session mode misclassification] -> Mitigation: establish explicit mode state and log one-time source-mode transition events.
- [Behavior change for users expecting native-size startup] -> Mitigation: document swapchain-extent default and keep manual prechain profile override unchanged.

## Migration Plan

1. Add policy type and resolver in `Application`; route existing toggle code through resolver.
2. Introduce stable session capture-mode state and use it for first-time per-surface defaults.
3. Add backend policy API and wire it to `FilterChain` stage controls.
4. Store/reapply policy in backend chain-init and chain-swap paths.
5. Refine prechain resolution/resource handling and history-source selection in `FilterChain`.
6. Make prechain startup default deterministic: initialize from swapchain extent in direct Vulkan sessions.
7. Gate compositor resize requests by effective policy transition/surface topology changes.
8. Validate with repeated startup runs (`pixi run start -p debug -- vkcube`) in addition to preset build/test flow.
9. Rollback strategy: revert to previous split setters if critical regressions occur; keep spec deltas to reattempt in a follow-up change.

## Open Questions

- None.
