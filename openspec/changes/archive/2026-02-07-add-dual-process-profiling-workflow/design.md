## Context

In profile builds, Goggles runtime behavior spans two instrumented processes:

1. app-side process with `goggles_vklayer`
2. viewer/compositor process (`goggles`)

Today, collection is manual and split. Tracy's CLI capture utility captures one client per invocation,
and Tracy tooling does not provide a native "merge two .tracy files into one timeline" command.

## Goals / Non-Goals

- Goals:
  - One command UX for profile sessions (`pixi run profile ... -- <app> ...`).
  - Capture both relevant Tracy clients in one session.
  - Emit a single merged timeline artifact for timeline-centric analysis.
  - Preserve original raw captures for deep per-process debugging.
- Non-Goals:
  - Perfect lossless merge of every Tracy data class (callstacks, lock graph internals, crash payloads,
    symbol transfer metadata).
  - Replacing `tracy-profiler` interactive workflows.

## Decisions

### Decision 1: New Pixi `profile` task with `start`-compatible CLI

Create `scripts/task/profile.sh` and wire `pixi run profile` to:
- parse `-p/--preset` (default `profile`)
- accept Goggles args before `--`
- accept app command/args after `--`
- run build/install prerequisites equivalent to the normal start flow

### Decision 2: Capture both Tracy clients automatically

Profile task launches two capture workers concurrently and assigns each output file to its client role:
- `viewer.tracy`
- `layer.tracy`

Implementation may use deterministic profiling endpoints and/or Tracy broadcast discovery, but must be
stable for repeated runs without manual port probing.

### Decision 3: Merge via normalized timeline pipeline

Because native `.tracy + .tracy -> .tracy` merge is unavailable, merge is defined as:

1. Extract timeline events from each raw trace.
2. Normalize into one multi-process timeline representation.
3. Build a single merged trace artifact from normalized events.

The merged artifact is timeline-focused and intended for frame/zone correlation across viewer and layer.
Raw traces remain the source of truth for full-fidelity single-process analysis.

### Decision 4: Cross-process alignment markers

Add frame-sequence markers in both processes so merge can align by shared frame identity when available.
Fallback behavior is required when shared markers are missing: merge still completes using relative time
origin with a warning in session metadata.

## Risks and Mitigations

- Risk: merge loses some data classes vs native trace
  - Mitigation: always keep and expose raw per-process traces.
- Risk: wrong client association during capture
  - Mitigation: explicit role mapping with validation metadata (client name/port/PID) in session manifest.
- Risk: tool availability drift
  - Mitigation: resolve/build required Tracy tooling from Pixi-managed workflow and fail fast with clear errors.

## Output Layout

Each session produces:
- `session.json` (metadata, command line, timestamps, client mapping, warnings)
- `viewer.tracy`
- `layer.tracy`
- `merged.tracy`
- optional intermediate normalized file(s) for troubleshooting
