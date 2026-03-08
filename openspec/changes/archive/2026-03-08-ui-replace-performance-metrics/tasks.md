## 1. Compositor metric source

- [x] 1.1 Add compositor-owned metric state that tracks active-surface gameplay cadence and commit-to-capture timing.
- [x] 1.2 Update the active-surface commit path in the XDG and XWayland compositor hooks so only the current capture target contributes to `Game FPS`.
- [x] 1.3 Publish `Compositor Latency` from the commit event through captured-frame publication without introducing a legacy timing fallback path.
- [x] 1.4 Introduce or update a compositor-to-application runtime metrics snapshot contract outside `src/ui` so the UI consumes already-computed values without depending on compositor internals.

## 2. App and UI integration

- [x] 2.1 Thread the compositor metric snapshot through the application runtime to the Application performance panel.
- [x] 2.2 Replace the `Render` / `Source` FPS UI with `Game FPS` and `Compositor Latency` in the Application performance panel.
- [x] 2.3 Remove the legacy performance plots, frame-history buffers, source-frame notify hook, and any now-unused code tied only to the retired metrics.

## 3. Contract and verification

- [x] 3.1 Verify the implementation still matches the `app-window` and `compositor-capture` delta specs with no legacy labels or hidden fallback metric path left behind.
- [x] 3.2 Run `pixi run build -p debug` and address any compile or contract drift issues.
- [x] 3.3 Run `pixi run build -p asan`, `pixi run test -p asan`, and `pixi run build -p quality`.
- [x] 3.4 Run `pixi run test -p test` when the compositor/UI runtime is available; otherwise perform a manual visible-panel check, record prerequisites and observations, and attach proof location for the fallback.
