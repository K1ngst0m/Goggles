# Change: Add Dual-Process Profiling Workflow

## Why

Profiling a normal Goggles launch currently requires manual Tracy orchestration: two independent client
processes (viewer/compositor and app-side Vulkan layer), two `tracy-capture` runs, and no first-class
way to produce one merged timeline artifact.

This slows down iterative performance work and makes profile collection error-prone.

## What Changes

- Add a user-facing `pixi run profile` task that follows the existing `pixi run start` argument pattern.
- Add deterministic dual-client capture orchestration for Goggles profile runs (viewer/compositor + app/layer).
- Produce two raw `.tracy` captures per session and a merged single-timeline trace artifact.
- Add cross-process frame-alignment markers to improve merge accuracy.
- Add structured output layout and diagnostics for failed/incomplete profile sessions.

## Impact

- Affected specs:
  - `profiling`
- Affected code:
  - `pixi.toml`
  - `scripts/task/help.sh`
  - `scripts/task/profile.sh` (new)
  - `scripts/profiling/` (new merge/capture helpers)
  - `src/app/application.cpp`
  - `src/capture/vk_layer/vk_capture.cpp`
  - build wiring for profiling-time Tracy endpoints/tooling availability
  - `README.md`
