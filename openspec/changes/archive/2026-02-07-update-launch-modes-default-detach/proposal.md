# Change: Update Launch UX With Default + Detach Modes

## Why

Launching a target app with Goggles' “full feature” workflow currently requires users to manually provide environment variables that are not stable or user-friendly (e.g., nested `DISPLAY`/`WAYLAND_DISPLAY` values and WSI proxy sizing). This is error-prone, hard to remember, and not suitable for packaging/Steam launch options.

## What Changes

- Add two run modes for the Goggles launch workflow:
  - **Default mode** (no `--detach`): “full feature” launch mode intended for typical use and packaging.
  - **Detach mode** (`--detach`): viewer-only mode intended for advanced/manual launching.
- In **default mode**:
  - Input forwarding is enabled and required.
  - `GOGGLES_WSI_PROXY=1` is forced for the launched app.
  - `DISPLAY` and `WAYLAND_DISPLAY` are set for the launched app (both are provided).
  - `--app-width` and `--app-height` are supported and only apply here (mapped to `GOGGLES_WIDTH`/`GOGGLES_HEIGHT`).
- In **detach mode**:
  - Input forwarding is disabled.
  - Default-mode-only options are rejected (`--app-width`, `--app-height`).
- Do not expose a `--wsi-proxy` toggle; WSI proxy is implicit in default mode.
- Ensure the launch flow continues to use configuration via `config/goggles.toml` (and `--config` overrides) for viewer behavior.

## Impact

- Affected specs:
  - `app-window` (CLI + launch-mode UX behavior)
- Affected code (expected):
  - `src/app/cli.hpp` / `src/app/main.cpp` / `src/app/application.*` (mode parsing + orchestration)
  - `scripts/task/start.sh` (optional: simplify to use new mode, reduce manual env usage)
  - `docs/` (update user-facing launch guidance)

## Non-Goals

- Changing Vulkan layer behavior or its environment variable contracts (only how the app sets them by default).
- Introducing a third “backend selection” mode (X11 vs Wayland); default mode always provides both.

## Open Questions

- Should the **app command** be required in default mode (packaging/Steam wrapper use), or optional (viewer-only default)?
  - This proposal assumes default mode is used for the “full feature” launch path and therefore expects an app command to be provided.

