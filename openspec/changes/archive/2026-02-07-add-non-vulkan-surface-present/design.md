## Context

The input compositor is headless and only forwards input while sending frame-done callbacks.
Non-Vulkan clients never appear in the Goggles viewer because no compositor render/capture path
exists. The viewer already knows how to import DMA-BUF frames from Vulkan capture, so the goal is
to reuse that pipeline for compositor-rendered frames.

## Goals / Non-Goals

- Goals:
  - Render the selected non-Vulkan surface into a headless output.
  - Export frames via DMA-BUF for zero-copy presentation.
  - Use the existing surface selector (manual override + focus fallback).
  - Keep input-only mode working if presentation cannot be enabled.
- Non-Goals:
  - No compositor-wide layout/tiling.
  - No changes to the Vulkan layer capture protocol.
  - No multi-surface blending or overlays.

## Decisions

- Render on the compositor thread using `wlr_render_pass` into a `wlr_swapchain` buffer.
- Export DMA-BUF attributes from the swapchain buffer and duplicate the FD for the viewer thread.
- Present only the selected surface (manual override or focused surface) per commit callback.
- Treat presentation as optional: if swapchain creation fails, continue input forwarding only.

## Risks / Trade-offs

- Renderer/export format support varies by backend; only single-plane linear DMA-BUFs are accepted.
- Surface commits can be more frequent than viewer renders; last-frame caching is used to decouple.

## Migration Plan

- No config changes required. Presentation is enabled when swapchain creation succeeds.
- Failure to create the swapchain leaves existing input forwarding behavior intact.

## Open Questions

- Should the headless output size be configurable rather than fixed (currently 1920x1080)?
- Do we need format negotiation beyond linear formats for broader hardware coverage?
