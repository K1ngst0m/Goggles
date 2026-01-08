# Design: Refactor App Main Orchestration

## Goals
- Make app startup/shutdown and per-frame orchestration explicit and maintainable.
- Keep behavior stable while moving logic out of `src/app/main.cpp`.
- Keep an “escape hatch” for future SDL app-callback entrypoints by exposing `handle_event()` + `tick_frame()` style APIs.

## Components

### `goggles::app::SdlPlatform`
Responsible for SDL initialization and window ownership.
- Owns `SDL_Init`/`SDL_Quit` lifetime via RAII.
- Owns `SDL_Window` creation/destruction.
- Provides non-owning access to `SDL_Window*` for subsystem factories.

### `goggles::app::UiController`
Responsible for ImGui integration glue and UI state reconciliation.
- Creates and owns `goggles::ui::ImGuiLayer` (optional; can be disabled).
- Maintains UI-only state that should not leak into the main loop (e.g., previous shader enabled state).
- Bridges UI events/state into `goggles::render::VulkanBackend` actions (reload preset, toggle shader, reset parameters).

### `goggles::app::Application`
Responsible for orchestration and app state.
- Owns major subsystems and optional components:
  - `VulkanBackend`
  - optional `CaptureReceiver` (viewer-only mode when unavailable)
  - optional `InputForwarder`
  - `UiController` (optional UI)
- Implements a stable driving surface:
  - `handle_event(const SDL_Event&)`
  - `tick_frame()`
  - `is_running()`

## Per-frame Phase Order (Invariant)
The refactor preserves the existing phase order to avoid subtle regressions:
1. Poll/process SDL events (ImGui first, then app hotkeys, then input forwarding if not captured)
2. Handle resize requests (event-driven and/or swapchain-driven)
3. Poll capture receiver and import/refresh sync semaphores when updated
4. Reconcile UI state into backend (preset reload, shader enabled)
5. Render either captured frame or clear; render UI via backend callback when enabled
6. Refresh UI parameter state when the filter chain swaps

## Future SDL App-Callback Wiring (Non-Goal)
This change does not adopt SDL app callbacks, but the `Application` surface is designed so a future adapter can forward:
- SDL events → `handle_event()`
- per-frame iterate → `tick_frame()`

