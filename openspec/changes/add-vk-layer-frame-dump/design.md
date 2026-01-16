## Context

Frame dumping is a debugging feature with non-trivial overhead (extra GPU work + disk I/O). The
capture layer is performance-sensitive and has strict constraints in hot paths like
`vkQueuePresentKHR`.

## Goals / Non-Goals

- Goals:
  - Enable opt-in dumping selected frames with zero overhead when disabled.
  - Support both WSI proxy and non-WSI-proxy paths.
  - Avoid file I/O and logging in `vkQueuePresentKHR`.
  - Emit image dumps and a sidecar metadata file without adding new dependencies.
- Non-Goals:
  - Support formats beyond `ppm` in the initial version.

## Decisions

- Decision: Dump selection is enabled only when `GOGGLES_DUMP_FRAME_RANGE` is non-empty.
- Decision: The “frame number” used for matching `GOGGLES_DUMP_FRAME_RANGE` is the frame number the
  layer uses for capture metadata (`CaptureFrameMetadata.frame_number`) for that frame.
- Decision: Dump output directory is configurable via `GOGGLES_DUMP_DIR` with default
  `/tmp/goggles_dump`.
- Decision: Each dumped frame produces `{processname}_{frameid}.ppm` and `{processname}_{frameid}.ppm.desc`.
  - The `.desc` file is plain text `key=value` pairs to avoid format dependencies.
- Decision: Any disk I/O is performed off-thread; the present thread only schedules GPU work and
  enqueues work items.

## Risks / Trade-offs

- Dumping can impact frametime; it is best-effort and intended for debugging.
- Correctness depends on handling swapchain formats and channel order; `ppm` requires RGB output
  and may require swizzling for common BGRA formats.
