# Change: Refactor App Main Orchestration

## Why
`src/app/main.cpp` currently mixes platform setup (SDL), subsystem initialization (Vulkan, UI, capture, input forwarding), per-frame orchestration, and shutdown logic in one large translation unit. This makes the entrypoint hard to maintain, hard to test, and risky to evolve (any feature tends to grow `main.cpp` further).

This change refactors the app entrypoint into a small set of focused components while preserving existing behavior and allowing small UX improvements (e.g., reducing duplicated resize handling) without introducing functional regressions.

## What Changes
- Introduce a dedicated app orchestrator (`goggles::app::Application`) that owns app state and coordinates:
  - SDL event handling (including F1 UI toggle, quit, resize)
  - optional capture polling + sync semaphore import
  - optional input forwarding with ImGui capture rules
  - UI state reconciliation and UI rendering integration
  - render/clear submission and resize recovery
- Introduce a minimal SDL platform wrapper for RAII lifetime management of:
  - `SDL_Init` / `SDL_Quit`
  - `SDL_Window` creation/destruction
- Introduce a UI controller to isolate ImGui-related glue:
  - preset discovery/catalog
  - filter-chain parameter ↔ UI state synchronization
  - applying UI actions to `VulkanBackend` (shader enable, preset reload, parameter reset)
- Reduce `src/app/main.cpp` to composition and top-level error boundary only.
- Keep the code structured so migrating to SDL3’s app-callback style later would be a wiring change (core logic exposes `handle_event()` and `tick_frame()` style methods).

## Impact
- Affected specs: `app-window`, `object-lifecycle`
- Affected code:
  - `src/app/main.cpp`
  - `src/app/` (new: `application.*`, `sdl_platform.*`, `ui_controller.*`)

## Non-Goals
- No new features or behavior changes in capture, render, or shader pipeline.
- No switch to SDL app-callback entrypoints in this change.
- No rework of VulkanBackend, CaptureReceiver, InputForwarder, or ImGuiLayer public APIs unless strictly required for extraction.

## Success Criteria
- `src/app/main.cpp` no longer contains per-frame orchestration logic or large helper blocks (UI glue, event routing, etc.).
- Initialization and shutdown paths have a single ownership model (RAII) with no duplicated cleanup sequences.
- Existing user-visible behaviors remain intact (window creation, quit behavior, F1 toggles UI visibility, optional capture/input forwarding).
- Error handling follows `docs/project_policies.md` (expected/Result-based, log once at boundaries).

## Risks & Mitigations
- **Risk:** Behavior regressions due to event ordering changes.
  - **Mitigation:** Keep the per-frame phase order identical during migration; document invariants in tasks and validate manually.
- **Risk:** Duplicate logging or silent failures introduced while moving code.
  - **Mitigation:** Enforce “log once at subsystem boundaries” during extraction; prefer propagating `Result` instead of converting to booleans.

