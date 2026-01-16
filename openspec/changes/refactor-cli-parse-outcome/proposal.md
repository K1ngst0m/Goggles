# Change: Refactor CLI Parsing With Explicit Parse Outcome

## Why

The current CLI parsing implementation (`goggles::app::parse_cli`) is a large inline function that
mixes option registration, argument splitting (`--`), exception handling, validation, and
normalization. This makes it difficult to maintain and easy to introduce duplicated or inconsistent
rules.

Additionally, “help/version requested” is currently represented as an error payload with
`ErrorCode::ok`, which complicates top-level error handling and blurs the meaning of `ErrorCode`.

## What Changes

- Introduce `goggles::app::CliParseOutcome` to represent CLI parsing results:
  - `action = run` with parsed `CliOptions`
  - `action = exit_ok` for `--help` / `--version` (and other “exit successfully” dispositions)
- Update CLI parsing so “exit requested” is expressed via `CliParseOutcome` rather than an
  `ErrorCode::ok` sentinel error.
- Refactor `parse_cli` implementation into small, testable helpers (argument splitting, option
  registration, parse, validation by mode, normalization) while preserving current CLI behavior.
- Move CLI parsing implementation out of `src/app/cli.hpp` into a `.cpp` translation unit to reduce
  header bloat and improve readability (header remains data + declarations).

## Impact

- Affected specs:
  - `app-window` (CLI behavior and parsing contract)
- Affected code (expected):
  - `src/app/cli.hpp` / `src/app/cli.cpp` (new) (public API + implementation split)
  - `src/app/main.cpp` (handle `CliParseOutcome` without sentinel errors)
  - `tests/app/test_cli.cpp` (update/extend tests for help/version and outcome handling)

## Non-Goals

- Changing the set of supported CLI flags or their semantics.
- Changing the project-wide `Result<T>` type implementation (`nonstd::expected` vs `tl::expected`).
- Changing launch-mode behavior beyond improving validation structure and readability.

## Open Questions

- Should `parse_cli` change signature directly to `Result<CliParseOutcome>`, or should a transitional
  wrapper be kept for `Result<CliOptions>` call sites?
- Should `CliParseOutcome::exit_ok` carry any optional metadata (e.g., “help” vs “version”), or is a
  single “exit successfully” action sufficient?

