# Change: Update File Logging Path Policy and Runtime Sink Setup

## Why
The configuration model already exposes `[logging].file`, but runtime startup does not attach a file sink,
so file logging is effectively non-functional today. In addition, relative log-file paths are ambiguous when
configuration is loaded from a location outside the current working directory.

Without a defined policy, log destination behavior can vary by launcher, shell, and packaging context,
which makes troubleshooting difficult and breaks portability.

## What Changes
- Define a deterministic path-resolution policy for `logging.file`:
  - empty value means console-only logging
  - absolute path is used as-is
  - relative path is resolved against the directory of the loaded config file
- Add startup behavior to configure an optional spdlog file sink when `logging.file` is non-empty.
- Define error handling for file-sink setup failures:
  - keep console logging active
  - log exactly one warning at app boundary
  - continue startup (non-fatal)
- Clarify docs/template comments for `[logging].file` semantics.
- Add unit/integration coverage for path resolution and sink activation behavior.

## Non-Goals
- No change to capture-layer logging behavior (`src/capture/vk_layer/`).
- No introduction of log rotation, retention policy, or size/time-based rollover in this change.
- No broad redesign of the logging macro surface.

## Impact
- Affected specs:
  - `app-window` (startup/config-driven logging behavior)
- Affected code (expected):
  - `src/util/logging.hpp`
  - `src/util/logging.cpp`
  - `src/util/config.hpp`
  - `src/util/config.cpp`
  - `src/app/main.cpp`
  - `config/goggles.template.toml`
  - `tests/util/test_logging.cpp`
  - `tests/util/test_config.cpp`

## Compatibility Notes
- Existing configs with `logging.file = ""` keep current console-only behavior.
- Existing configs with absolute file paths continue to work with deterministic behavior.
- Existing configs with relative file paths become stable and CWD-independent by resolving relative to
  the config file directory.
