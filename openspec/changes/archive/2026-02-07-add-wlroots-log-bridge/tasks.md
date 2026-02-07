## 1. Implementation
- [x] 1.1 Initialize wlroots logging with a project log callback during compositor startup (before backend/XWayland creation).
  - Implemented in src/compositor/compositor_server.cpp (wlroots log bridge + init hook).
  - Verified by `pixi run build -p debug`.
- [x] 1.2 Map wlroots importance levels to GOGGLES_LOG_* levels and respect configured verbosity.
  - Implemented in src/compositor/compositor_server.cpp (importance mapping + logger bridge).
  - Verified by `pixi run build -p debug`.
- [x] 1.3 Ensure stderr suppression remains scoped to external helper noise without hiding wlroots logs.
  - Implemented in src/compositor/compositor_server.cpp (wlroots logs routed through project logger).
  - Verified by `pixi run build -p debug`.

## 2. Verification
- [x] 2.1 Manual test: run with default log level and confirm wlroots debug logs are filtered while info/error logs appear via project logger.
- [x] 2.2 Manual test: run with debug/trace and confirm wlroots debug logs appear via project logger.
