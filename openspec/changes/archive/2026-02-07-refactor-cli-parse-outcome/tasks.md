## 1. Implementation
- [x] 1.1 Introduce `CliAction` + `CliParseOutcome` in `src/app/cli.hpp`
- [x] 1.2 Refactor `parse_cli` to return `Result<CliParseOutcome>` (remove `ErrorCode::ok` sentinel)
- [x] 1.3 Move CLI parsing implementation into `src/app/cli.cpp` and slim `src/app/cli.hpp`
- [x] 1.4 Split parsing into helpers: argv split, option registration, parse, validation, normalize
- [x] 1.5 Update `src/app/main.cpp` to handle `CliParseOutcome` (`exit_ok` -> `EXIT_SUCCESS`)

## 2. Tests
- [x] 2.1 Update `tests/app/test_cli.cpp` for new `parse_cli` return type
- [x] 2.2 Add tests for `--help` and `--version` returning `exit_ok` as success
- [x] 2.3 Ensure existing validation tests remain passing (detach restrictions, `--` separator rules)

## 3. Validation
- [x] 3.1 `pixi run format`
- [x] 3.2 `pixi run build -p test` (covered by `pixi run test -p test`)
- [x] 3.3 `pixi run test -p test`

## 4. OpenSpec Hygiene
- [x] 4.1 Run `openspec validate refactor-cli-parse-outcome --strict`
