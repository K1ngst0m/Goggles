## Context
`[logging].file` is parsed into configuration state but not currently applied to logger sink setup.
The logger is initialized with a console sink only, and startup applies only log level and timestamp
configuration. This leaves file logging non-functional and path semantics undefined.

## Goals / Non-Goals
- Goals:
  - Define deterministic and launcher-independent `logging.file` resolution behavior.
  - Make file logging operational without regressing console logging.
  - Keep startup robust: file logging failures are visible but non-fatal.
- Non-Goals:
  - Log rotation or archival behavior.
  - Changes to capture-layer logging backend/policy.

## Decisions
- Decision: `logging.file` resolution policy
  - Empty string: disable file sink (console-only).
  - Absolute path: use as-is.
  - Relative path: resolve against the parent directory of the loaded config file.
- Rationale:
  - Resolving relative to config origin is stable across CWD changes and matches common config-driven
    path semantics used by mature tooling.

- Decision: startup sequencing
  - Keep early console logger initialization for bootstrap diagnostics.
  - After config load succeeds, resolve `logging.file` and attach optional file sink.
  - Apply level/timestamp and continue startup.
- Rationale:
  - Preserves visibility for early startup messages while enabling config-driven sink behavior before
    subsystem construction.

- Decision: failure handling
  - If file sink creation/open fails, log one warning to console and continue with console sink only.
- Rationale:
  - Logging destination misconfiguration should not block application launch.

## Alternatives Considered
- Alternative: resolve relative `logging.file` to CWD.
  - Rejected: behavior depends on launcher and invocation directory.
- Alternative: require absolute path only.
  - Rejected: reduces usability and portability of checked-in/shared config files.
- Alternative: resolve relative path to XDG cache/data dir.
  - Rejected: disconnects configured file from config location and is surprising for explicit paths.

## Risks / Trade-offs
- Early bootstrap logs emitted before sink attachment may not be present in the log file.
  - Mitigation: keep this behavior explicit; optionally add a follow-up change for replay/buffer if required.
- Relative path semantics change for users who implicitly relied on CWD behavior.
  - Mitigation: document behavior in template/docs and add tests covering non-CWD config loads.

## Migration Plan
1. Add spec requirements and implementation tasks.
2. Implement runtime sink setup and path resolution wiring.
3. Add regression tests for sink activation and path semantics.
4. Update template/docs and release notes.

## Open Questions
- Whether to add a dedicated app log directory policy (for example under XDG state) as a follow-up change.
