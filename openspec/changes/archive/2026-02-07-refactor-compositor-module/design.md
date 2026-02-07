## Context
The compositor server has expanded beyond input forwarding to include compositor-presented surface frame capture. The existing InputForwarder wrapper now mirrors most of CompositorServer and obscures ownership boundaries.

## Goals / Non-Goals
- Goals:
  - Make compositor responsibilities explicit with a dedicated module.
  - Eliminate redundant wrapper layers around the compositor server.
  - Keep Wayland/wlroots details encapsulated behind an Impl.
- Non-Goals:
  - Changing compositor behavior or input semantics.
  - Introducing new capture backends or altering IPC protocols.

## Decisions
- Decision: Create `src/compositor/` module and move CompositorServer there.
  - Why: It reflects the expanded scope (input + surface frames) and avoids miscategorizing compositor logic as input-only.
- Decision: Merge InputForwarder API into CompositorServer (SDL event forwarding methods).
  - Why: The wrapper provides no additional abstraction; merging reduces indirection while preserving the Impl boundary.
- Decision: Move generated Wayland protocol headers into `src/compositor/protocol/`.
  - Why: Keep compositor module organized and clearly separate generated protocol artifacts.

## Risks / Trade-offs
- SDL types become part of CompositorServer's public API; this increases module coupling to SDL.
  - Mitigation: Keep SDL usage limited to forwarding methods; preserve internal input event types and queue.

## Migration Plan
- Move compositor files and update includes/targets.
- Delete InputForwarder files and update app call sites to use CompositorServer directly.
- Update CMake target names/links.
- Relocate generated protocol headers and ensure include paths include the protocol subdir.

## Open Questions
- Should SDL translation remain in compositor module long-term, or be split into a thin SDL helper later?
