## 1. Spec & Design
- [ ] Add `config-loading` capability delta spec (new).
- [ ] Add `packaging` delta spec updates for stable resource root + CWD independence.
- [ ] Document path resolution interfaces and precedence rules in `design.md`.

## 2. Implementation (C++ / src only)
- [ ] Add `src/util/paths.hpp/.cpp` that resolves `resource_dir`, `config_dir`, `data_dir`, `cache_dir`, `runtime_dir` via `Result<T>` APIs.
- [ ] Extend `src/util/config.hpp` with `[paths]` root overrides (resource/config/data/cache/runtime) and update `src/util/config.cpp` parser/validation.
- [ ] Update `src/app/main.cpp` to:
  - [ ] Resolve config location early (bootstrap)
  - [ ] Load + validate config early
  - [ ] On failure, log warning once and fall back to defaults (or template) per spec
  - [ ] Use resolved `resource_dir` for shipped assets when packaged (no CWD dependency)
- [ ] Replace direct env/XDG path sniffing in `src/` call sites with `goggles::util` path resolver usage (follow-up work; keep minimal and incremental).

## 3. Testing & Validation
- [ ] Add/extend unit tests for config loading fallback behavior and path override parsing (tests mirror `src/` structure).
- [ ] Run `pixi run build -p test` and `pixi run test -p test`.
- [ ] Run `pixi run format`.

## 4. Docs / Templates
- [ ] Update the shipped config template (`config/goggles.template.toml`) to include commented `[paths]` overrides.
- [ ] Ensure logging and error handling conforms to `docs/project_policies.md` (no cascading logs, `Result<>` return types).

