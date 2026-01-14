# Design: Central Path Resolution + Early Config Validation

## Purpose
Define a minimal, consistent API for resolving filesystem directory roots and for bootstrapping
configuration load/validation with deterministic fallback behavior across Linux systems and AppImage.

## Interfaces (High-Level)

### Directory Roots (only)
All “where do we read/write” decisions are expressed in terms of these roots:
- `resource_dir` (read-only, packaged assets/templates/shaders/fonts)
- `config_dir` (user config location)
- `data_dir` (user data: manifests, installed shader packs, etc.)
- `cache_dir` (user cache: shader cache, etc.)
- `runtime_dir` (ephemeral runtime state)

### Types
`goggles::util::PathOverrides`
- Optional overrides for the five directory roots.
- Source precedence is defined by the bootstrap flow (CLI > config > env > defaults).

`goggles::util::AppDirs`
- Concrete resolved paths for the five directory roots.
- Naming is consistent: `*_dir` for directories.

### Functions
Bootstrap helpers:
- `resolve_config_dir(overrides) -> Result<path>`
- `resolve_app_dirs(ctx, overrides) -> Result<AppDirs>`

Override helpers:
- `overrides_from_config(config) -> PathOverrides`
- `merge_overrides({high, low}) -> PathOverrides` (field-wise “first non-empty wins”)

Join helpers (avoid ad-hoc concatenation at call sites):
- `resource_path(dirs, rel) -> path`
- `config_path(dirs, rel) -> path`
- `data_path(dirs, rel) -> path`
- `cache_path(dirs, rel) -> path`
- `runtime_path(dirs, rel) -> path`

## Resolution Rules

### XDG Compliance
For `config_dir`, `data_dir`, `cache_dir`, `runtime_dir`:
1) explicit override (CLI/config)
2) XDG env (`XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_CACHE_HOME`, `XDG_RUNTIME_DIR`)
3) HOME fallbacks (`$HOME/.config`, `$HOME/.local/share`, `$HOME/.cache`)
4) last-resort fallback only where unavoidable (runtime may fall back to a temp location)

### Packaged Resource Root (AppImage-consistent)
For `resource_dir`, resolution MUST NOT depend on CWD for packaged runs. Precedence:
1) explicit override (CLI/config)
2) `GOGGLES_RESOURCE_DIR` env override (expected in packaged wrapper)
3) packaged/exe-derived candidates (validated by sentinel existence)
4) optional developer fallback (`cwd`) only in non-packaged dev workflows / last-resort

Sentinel validation SHOULD use a small set of required paths (e.g. config template and shaders)
to avoid accepting incorrect directories.

## Config Loading: Validate Early + Fallback

### Boot Sequence (two-phase)
1) Build CLI overrides (if present).
2) Resolve `config_dir` (enough to locate the config file candidate).
3) Load + validate config early.
4) Merge overrides: `cli > config`.
5) Resolve full `AppDirs`.

### Fallback Behavior
- If config is missing or invalid in non-strict flows, the app SHALL:
  - log a single warning at the app boundary
  - fall back to defaults and continue
- If a template config exists under `resource_dir`, the app MAY attempt to write a user config
  into `config_dir` atomically; on failure, it SHALL continue using defaults/template with a warning.

## Policy Compliance
- All fallible operations return `Result<T>` and do not use exceptions for expected failures.
  If a third-party library throws (e.g. TOML parser), catch at the boundary and convert to `Error`.
- Log errors once at subsystem boundaries (no cascading logs).
