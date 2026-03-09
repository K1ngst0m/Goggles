## 1. Runtime metrics contract

- [x] 1.1 Extend the shared compositor-to-application runtime metrics contract so it carries current
  `Game FPS` and `Compositor Latency` values plus bounded plot-ready history for both metrics.
- [x] 1.2 Update compositor metric collection so the published histories are populated from the active
  capture target only and reset cleanly when the target changes.

## 2. Application and UI restoration

- [x] 2.1 Thread the expanded runtime metrics snapshot through the application boundary into
  `ImGuiLayer` without reintroducing any legacy timing path.
- [x] 2.2 Restore one live `Game FPS` plot beneath the existing `Game FPS` text readout in the
  Application performance panel.
- [x] 2.3 Restore one live `Compositor Latency` plot beneath the existing latency text readout in the
  Application performance panel.
- [x] 2.4 Verify the panel continues to omit legacy `Render` / `Source` labels, plots, and timing
  buffers.

## 3. Verification

- [x] 3.1 Verify the implementation matches the `app-window` and `compositor-capture` delta specs and
  keeps the compositor-sourced metrics path as the only source of truth.
- [x] 3.2 Verify `Game FPS` and `Compositor Latency` histories clear before accumulating samples for a
  new capture target, using automated coverage when available or a named manual proof artifact when
  fallback is required.
- [x] 3.3 Run `pixi run build -p debug` and address any compile or contract drift issues.
- [x] 3.4 Run `pixi run build -p asan`, `pixi run test -p asan`, and `pixi run build -p quality`.
- [x] 3.5 Run `pixi run test -p test` when the compositor/UI runtime is available; otherwise perform a
  manual visible-panel validation, record prerequisites, target-switch observations, and attach proof
  location for the fallback.
