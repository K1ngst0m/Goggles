## 1. Implementation

- [x] 1.1 Add `pixi run profile` task and help text, matching the existing `start` argument split pattern.
- [x] 1.2 Implement `scripts/task/profile.sh` orchestration:
  - build/manifest prerequisites
  - target launch
  - dual Tracy capture lifecycle
  - deterministic session output directory
- [x] 1.3 Add/resolve Tracy tooling required by the profile workflow in a Pixi-compatible way.
- [x] 1.4 Add cross-process frame alignment markers in profiled code paths:
  - viewer-side source frame marker
  - layer-side captured frame marker
- [x] 1.5 Implement merge pipeline that consumes two raw traces and emits one merged timeline trace artifact.
- [x] 1.6 Write session metadata manifest (`session.json`) with client mapping, warnings, and artifact paths.
- [x] 1.7 Update docs (`README.md`) with profile usage examples and artifact explanation.

## 2. Verification

- [x] 2.1 `pixi run help` lists the new `profile` task.
- [x] 2.2 `pixi run profile --help` documents options and argument split behavior.
- [x] 2.3 Manual run: `pixi run profile -p profile -- <app>` produces `viewer.tracy` and `layer.tracy` in one session directory.
- [x] 2.4 Manual run produces `merged.tracy` with both process timelines visible.
- [x] 2.5 Validate fallback path by forcing missing alignment markers and confirming merge warning + successful artifact generation.
- [x] 2.6 Validate profile task returns non-zero on capture/merge hard failures with actionable diagnostics.
