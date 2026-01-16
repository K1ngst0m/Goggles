# Proposal: Refactor Vulkan implicit-layer logging

## Why
`src/capture/vk_layer/` is an implicit Vulkan layer that can be injected into arbitrary host
applications. Logging must be:
- Explicitly opt-in (avoid polluting host output by default)
- Low overhead when enabled, and near-zero overhead when disabled
- Robust against log storms (anti-spam)

The current approach is based on ad-hoc `fprintf` usage and does not provide runtime level control
or anti-spam behavior.

## What Changes
- Add runtime logging controls for the Vulkan layer:
  - `GOGGLES_DEBUG_LOG=1` enables layer logging (default: off)
  - `GOGGLES_DEBUG_LOG_LEVEL=<level>` sets the minimum level (default when enabled: `info`)
- Add Goggles CLI flags to forward these environment variables when launching a target app:
  - `--layer-log`
  - `--layer-log-level <level>`
- Move layer logging to a `write(2)`-based backend (no `stdio`).
- Add anti-spam macros (e.g., “log once”, “log every N”).
- Preserve the existing call-site interface (`LAYER_DEBUG`, `LAYER_WARN`, `LAYER_ERROR`,
  `LAYER_CRITICAL`) and keep `vkQueuePresentKHR` free of logging.

## Non-Goals
- Switching the Vulkan layer to the app’s `spdlog` backend (layer-only builds must stay standalone).
- Adding new logging call sites (this change focuses on the backend and controls).

## Impact
- Behavior change: layer logs are suppressed unless explicitly enabled with `GOGGLES_DEBUG_LOG=1`.
- When enabled, logging honors `GOGGLES_DEBUG_LOG_LEVEL` filtering.
- The log prefix remains `[goggles_vklayer]` for easy filtering.

## Open Questions
- `docs/project_policies.md` currently states capture layer logs should be `error/critical` only, while
  `openspec/specs/vk-layer-capture/spec.md` previously allowed `info` logs during initialization.
  This change keeps logs opt-in and level-filtered, but we should confirm the desired policy:
  - Strictly enforce `error/critical` only even when debug logging is enabled, or
  - Allow `info/warn/debug` when `GOGGLES_DEBUG_LOG=1` is explicitly set.

## Testing
- Add unit tests covering:
  - Env var parsing (unset/defaults, invalid values, case/whitespace tolerance if supported)
  - Level filtering behavior
  - Anti-spam macros (once / every-N)
  - Output formatting and truncation edge cases
