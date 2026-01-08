# Tasks: Refactor App Main Orchestration

## 1. Preparation (no behavior change)
- [x] Confirm current invariants in `src/app/main.cpp` (event priority, resize handling, UI toggle, input forwarding capture rules).
- [x] Confirm all new fallible init uses `Result`/`ResultPtr` per `docs/project_policies.md`.

## 2. Extract UI Glue (behavior-preserving move)
- [x] Create `src/app/ui_controller.hpp/.cpp` and move ImGui/preset/parameter glue out of `main.cpp`.
- [x] Eliminate hidden static UI state by storing prior UI state in the controller.
- [x] Hide SDL/ImGui header dependencies (forward decls + out-of-line definitions).

## 3. Add SDL Platform RAII (reduce cleanup duplication)
- [x] Create `src/app/sdl_platform.hpp/.cpp` for SDL init/quit and window lifetime management.
- [x] Replace early-return cleanup branches with RAII-managed teardown.
- [x] Use a `CreateInfo` struct for SDL platform/window creation to avoid boolean/positional arg churn.

## 4. Introduce `Application` Orchestrator (explicit state + per-frame phases)
- [x] Create `src/app/application.hpp/.cpp` with `create(...) -> ResultPtr<Application>`.
- [x] Move ownership of window/backend/ui/capture/input into `Application`.
- [x] Implement `handle_event(const SDL_Event&)` and `tick_frame()` preserving current phase order:
  - events → resize flagging → capture poll → semaphore import → UI begin/end → render/clear → resize recovery → UI parameter refresh on chain swap

## 5. Shrink Entrypoint
- [x] Update `src/app/main.cpp` to only compose config/CLI and drive the loop via `Application`.
- [x] Keep top-level exception boundary in `main()` (no exceptions for expected failures).

## 6. Specs
- [x] Add spec deltas for `app-window` and `object-lifecycle` reflecting the new orchestration and RAII ownership requirements.

## 7. Validation
- [x] Build (debug + ASAN preset if available).
- [x] Manual checks:
  - window opens and closes cleanly; quit exits loop and cleans SDL resources
  - F1 toggles ImGui visibility
  - shader preset loads (config + CLI override)
  - viewer-only mode works when capture receiver fails
  - input forwarding only forwards when ImGui does not capture input
