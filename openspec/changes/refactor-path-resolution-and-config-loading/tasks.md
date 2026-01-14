## 1. Spec & Design
- [x] Add `config-loading` capability delta spec (new).
- [x] Add `packaging` delta spec updates for stable resource root + CWD independence.
- [x] Document path resolution interfaces and precedence rules in `design.md`.

## 2. Implementation (C++ / src only)
- [x] Add `src/util/paths.hpp/.cpp` that resolves `resource_dir`, `config_dir`, `data_dir`, `cache_dir`, `runtime_dir` via `Result<T>` APIs.
- [x] Extend `src/util/config.hpp` with `[paths]` root overrides (resource/config/data/cache/runtime) and update `src/util/config.cpp` parser/validation.
- [x] Update `src/app/main.cpp` to:
  - [x] Resolve config location early (bootstrap)
  - [x] Load + validate config early
  - [x] On failure, log warning once and fall back to defaults (or template) per spec
  - [x] Use resolved `resource_dir` for shipped assets when packaged (no CWD dependency)
- [x] Replace remaining direct env/XDG path sniffing in `src/` call sites with `goggles::util` path resolver usage (follow-up; incremental).

## 3. Testing & Validation
- [x] Add unit tests for path resolution behavior (mirrors `src/` structure).
- [x] Add unit tests for config loading fallback behavior (explicit `--config` invalid/missing).
- [x] Run `pixi run format`.
- [x] Run `pixi run dev -p quality`.

## 4. Docs / Templates
- [x] Update the shipped config template (`config/goggles.template.toml`) to include commented `[paths]` overrides.
- [x] Ensure logging and error handling conforms to `docs/project_policies.md` (no cascading logs, `Result<>` return types).
