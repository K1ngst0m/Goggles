# ci Spec Delta

## Purpose
Document CI behavior changes tied to Pixi-managed formatting and safe handling of forked pull requests.

## Requirements

## MODIFIED Requirements

### Requirement: Auto-format Code on Push

The CI system SHALL automatically format code using the Pixi-managed clang-format and gate subsequent jobs on whether formatting changes were pushed.

#### Scenario: Code with formatting issues is pushed
- **WHEN** code with clang-format violations is pushed to a branch
- **THEN** CI runs clang-format to fix the issues via Pixi
- **AND** CI commits the formatted code with message \"style: auto-format code\" when the branch is non-fork
- **AND** the format check job succeeds

#### Scenario: Forked PR formatting without push
- **GIVEN** a pull request originates from a fork
- **WHEN** clang-format produces changes
- **THEN** CI SHALL skip auto-commit/push for safety
- **AND** it SHALL expose `formatted=false` so downstream jobs still run

#### Scenario: Code is already properly formatted
- **WHEN** code that passes clang-format check is pushed
- **THEN** CI detects no changes needed
- **AND** no commit is created
- **AND** the format check job succeeds

#### Scenario: All C/C++ file types are formatted
- **WHEN** clang-format is run in CI
- **THEN** files with extensions `.c`, `.h`, `.cpp`, `.hpp` are formatted
- **AND** the same clang-format version from Pixi is used
