# Change: Refactor Path Resolution and Config Loading

## Why
Goggles currently resolves filesystem paths (resource/config/data/cache/runtime) in multiple places,
mixing environment variables, XDG defaults, and working-directory-relative fallbacks. This can lead to
inconsistent behavior across Linux systems and packaged runs (AppImage), and makes it hard to
centralize filesystem access.

Config loading also needs a clearly defined, early validation and fallback strategy so that:
- invalid or missing configs do not cause surprising failures in non-strict flows
- errors are logged once at the right boundary (per `docs/project_policies.md`)

## What Changes
- Introduce a single `goggles::util` path resolution API that resolves **only** these directory roots:
  `resource_dir`, `config_dir`, `data_dir`, `cache_dir`, `runtime_dir`.
- Add optional config-driven overrides for those five roots via `[paths]` in the TOML config.
- Define a two-phase bootstrap flow:
  1) resolve config dir + config file candidate early
  2) load + validate config; on failure, fall back with a warning (including when `--config` is
     explicitly provided)
- Update runtime to avoid relying on CWD for shipped assets when packaged (AppImage), consistent with
  the existing `packaging` spec requirement “Packaged Assets Are Not CWD-Dependent”.

## Non-Goals
- Centralizing every file open/read/write call behind a virtual filesystem interface.
- Adding leaf-specific overrides (e.g., per-file paths). Only directory roots are in scope.
- Changing shader/preset semantics or adding new user-facing features beyond path resolution and
  config load behavior.

## Impact
- Affected specs:
  - `packaging` (clarify resource root expectations and CWD independence)
  - New capability: `config-loading` (define requirements for config discovery/validation/fallback and
    path overrides)
- Affected code (expected):
  - `src/app/main.cpp` (bootstrap + config load flow)
  - `src/util/config.*` (parse `[paths]` and expose overrides)
  - New module in `src/util/` for path resolution
  - Call sites in `src/` that currently resolve paths ad-hoc (follow-up refactor task)

## Compatibility Notes
- Existing config files remain valid; `[paths]` is optional.
- Default behavior continues to follow XDG conventions; overrides only narrow behavior.
- Config parse/validation failures fall back with `warn` logging, including when `--config` is
  explicitly provided (the explicit config is ignored and defaults are used).
