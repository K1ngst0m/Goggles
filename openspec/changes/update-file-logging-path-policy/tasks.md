## 1. Spec & Design
- [x] 1.1 Add `app-window` delta requirements for file sink setup and relative path resolution rules.
  - Implemented in `openspec/changes/update-file-logging-path-policy/specs/app-window/spec.md`.
  - Verified by `openspec validate update-file-logging-path-policy --strict`.
- [x] 1.2 Document runtime sequencing and fallback policy in `design.md`.
  - Implemented in `openspec/changes/update-file-logging-path-policy/design.md`.
  - Verified by design review against implemented startup flow in `src/app/main.cpp`.

## 2. Implementation
- [x] 2.1 Extend logging initialization APIs to support optional file sink configuration using resolved path input.
  - Implemented `set_log_file_path()` in `src/util/logging.hpp` and `src/util/logging.cpp`.
  - Verified by `tests/util/test_logging.cpp` file sink success/failure coverage.
- [x] 2.2 Track config source path in startup flow so `logging.file` relative paths can be resolved against the config file directory.
  - Implemented `LoadedConfig` source-path tracking in `src/app/main.cpp`.
  - Verified by startup path resolution flow and `resolve_logging_file_path()` tests.
- [x] 2.3 Update startup ordering to apply log level, timestamp, and file sink after config load and before major subsystem initialization.
  - Implemented log config application sequence in `src/app/main.cpp` before `Application::create`.
  - Verified by `pixi run test -p test` and runtime log output in test harness.
- [x] 2.4 Implement non-fatal fallback when file sink setup fails: keep console sink, emit one warning, continue startup.
  - Implemented warning-on-failure path in `src/app/main.cpp` and non-throwing `set_log_file_path()` result API.
  - Verified by `tests/util/test_logging.cpp` invalid destination test.
- [x] 2.5 Update config/template comments to define `logging.file` semantics (empty/absolute/relative).
  - Implemented in `config/goggles.template.toml` and `docs/project_policies.md`.
  - Verified by doc/template content review.

## 3. Verification
- [x] 3.1 Add tests for `logging.file` path resolution using config files loaded from non-CWD locations.
  - Added `resolve_logging_file_path` tests in `tests/util/test_config.cpp`.
  - Verified by `pixi run test -p test`.
- [x] 3.2 Add tests confirming file sink is enabled when valid path is provided and logs are written.
  - Added file sink write/read assertion test in `tests/util/test_logging.cpp`.
  - Verified by `pixi run test -p test`.
- [x] 3.3 Add tests confirming invalid/unwritable file paths do not abort startup and preserve console logging.
  - Added failing-path test in `tests/util/test_logging.cpp` checking error result and continued logger usability.
  - Verified by `pixi run test -p test`.
- [x] 3.4 Run `pixi run test -p test`.
  - Executed successfully (all tests passed).
  - Quality follow-up also passed via `pixi run build -p quality`.

## 4. Documentation
- [x] 4.1 Update user-facing configuration guidance for `[logging].file` path policy.
  - Updated guidance in `config/goggles.template.toml` and `docs/project_policies.md`.
  - Verified by content review and formatter pass (`pixi run format`).
- [x] 4.2 Ensure docs explicitly state that this change applies to app logging, not vk-layer logging.
  - Added explicit scope note in `docs/project_policies.md` section B.5.
  - Verified by `openspec validate update-file-logging-path-policy --strict`.
