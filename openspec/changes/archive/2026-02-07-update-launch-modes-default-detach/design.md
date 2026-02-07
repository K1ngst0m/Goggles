# Design Notes: Default + Detach Launch Modes

## Goals

- Provide a “full feature” launch path that is short and packable (Steam/AppImage friendly).
- Avoid exposing unstable runtime details (nested compositor display identifiers, WSI proxy env toggles).
- Encode option dependencies in the CLI so invalid combinations are rejected early.

## Mode Definitions

### Default Mode (no `--detach`)

Default mode is the “full feature” launcher path:

- Viewer starts with input forwarding enabled.
- The app is launched by Goggles (same entrypoint) with:
  - `GOGGLES_CAPTURE=1`
  - `GOGGLES_WSI_PROXY=1` (forced)
  - `DISPLAY` and `WAYLAND_DISPLAY` set (both provided)
  - Optional size:
    - `--app-width` → `GOGGLES_WIDTH`
    - `--app-height` → `GOGGLES_HEIGHT`

### Detach Mode (`--detach`)

Detach mode is viewer-only:

- Input forwarding is disabled.
- Default-mode-only options are rejected (`--app-width`, `--app-height`).
- The user (or external launcher) can start the target app independently.

## Config Support

- The viewer continues to load configuration from `config/goggles.toml` by default.
- CLI `--config` overrides are still supported.
- Mode-specific behavior (default/detach) may override config values when the dependency graph requires it (e.g., detach mode disables input forwarding even if config enables it).

## CLI Dependency Rules (hard requirements)

- `--detach` MUST be mutually exclusive with `--app-width` and `--app-height`.
- Default mode MUST enable input forwarding.
- Default mode MUST set `GOGGLES_WSI_PROXY=1` for the launched app.

