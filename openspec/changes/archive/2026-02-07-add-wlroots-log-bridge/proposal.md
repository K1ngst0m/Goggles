# Change: Add wlroots Log Bridge

## Why
The compositor server currently relies on stderr output from wlroots/XWayland, which is suppressed at non-debug log levels. This bypasses the project's logging configuration and can hide actionable errors.

## What Changes
- Route wlroots logs through the project logger via a wlroots log callback.
- Map wlroots log importance to project log levels and respect configured verbosity.
- Keep stderr suppression limited to external helper noise (e.g., XWayland/xkbcomp), while ensuring wlroots logs remain visible via the project logger.

## Impact
- Affected specs: input-forwarding
- Affected code: src/compositor/compositor_server.cpp, src/util/logging.*
