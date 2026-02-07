# Design: CLI parsing refactor + `CliParseOutcome`

## Goals

- Remove the `ErrorCode::ok` sentinel used for “help/version requested”.
- Preserve existing CLI behavior (flags, validation rules, error messages where feasible).
- Improve readability by splitting the CLI logic into focused helpers with clear responsibilities.
- Reduce header bloat by moving CLI11-heavy parsing code out of `src/app/cli.hpp`.
- Keep “no exceptions into the main execution flow” by catching CLI11 parse exceptions and returning
  `Result<>` values.

## Non-Goals

- Changing CLI11 as the parsing library.
- Broadly migrating the project to a different `expected` implementation.
- Reworking application startup orchestration beyond the CLI parse boundary.

## Proposed API

Add a value type representing the CLI parse disposition:

- `enum class CliAction { run, exit_ok };`
- `struct CliParseOutcome { CliAction action; CliOptions options; };`

Notes:
- For `action == exit_ok`, `options` MAY be default-initialized and MUST NOT be consumed by callers.
- `parse_cli(argc, argv)` returns `Result<CliParseOutcome>`.
  - `exit_ok` is returned as a *successful* result.
  - Parse failures return `ErrorCode::parse_error` and a human-readable message.

## Internal Structure (Implementation Outline)

Split the current monolithic flow into helpers:

1) `split_argv_on_separator(argc, argv)`
   - Finds `--` once and returns:
     - `viewer_argc` (argv span presented to CLI11)
     - `has_separator`
     - `app_args` (vector of strings after `--`)

2) `register_options(CLI::App&, CliOptions&)`
   - Pure wiring: defines CLI options/flags and validation checks.
   - No mode-specific logic.

3) `parse_viewer_args(CLI::App&, viewer_argc, argv) -> Result<CliParseDisposition>`
   - Catches `CLI::ParseError`.
   - Uses `app.exit(e)` to trigger CLI11 printing.
   - Returns:
     - `exit_ok` when CLI11 indicates success exit (help/version).
     - `parse_error` otherwise.

4) `validate_by_mode(options, has_separator, app_args) -> Result<void>`
   - Encodes “detach vs default mode” invariants:
     - Detach: rejects default-mode-only flags and rejects app command.
     - Default mode: requires `--` and a non-empty app command.

5) `normalize(options)`
   - Applies derived fields consistently:
     - `--layer-log-level` implies `--layer-log`.
     - Width/height pairing invariant enforced (or represented as a single optional “dimensions”
       concept and normalized into the existing fields for downstream compatibility).

`parse_cli` becomes a small orchestration function calling these helpers in order.

## Alternatives Considered

1) Keep `ErrorCode::ok` sentinel in the error channel
   - Minimal code churn, but retains an awkward contract (success encoded as “error”).

2) Introduce a separate `parse_cli_or_exit(...)` function
   - Clarifies intent but duplicates parsing logic or forces extra call-site decisions.

3) Return `std::optional<CliOptions>` with out-params for errors (rejected)
   - Violates project error handling conventions and loses structured error reporting.

Chosen: `CliParseOutcome` as a value-based, explicit disposition.

## Testing Strategy

- Extend `tests/app/test_cli.cpp` to cover:
  - `--help` and `--version` return `action == exit_ok` as success (no `ErrorCode::ok` in error).
  - Existing detach/default mode validation remains unchanged.
  - `--layer-log-level` implies `--layer-log` normalization.

