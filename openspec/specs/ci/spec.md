# ci Specification

## Purpose
Defines the Continuous Integration pipeline behavior for the Goggles project, including automated code formatting, building, testing, and static analysis.
## Requirements
### Requirement: Auto-format Code on Push

The CI system SHALL automatically format code using clang-format and commit the changes instead of failing the build on formatting issues.

#### Scenario: Code with formatting issues is pushed
- **WHEN** code with clang-format violations is pushed to a branch
- **THEN** CI runs clang-format to fix the issues
- **AND** CI commits the formatted code with message "style: auto-format code"
- **AND** the format check job succeeds

#### Scenario: Code is already properly formatted
- **WHEN** code that passes clang-format check is pushed
- **THEN** CI detects no changes needed
- **AND** no commit is created
- **AND** the format check job succeeds

### Requirement: Scoped Sanitizer Instrumentation

The build system SHALL apply sanitizer instrumentation only to first-party Goggles code, excluding all third-party dependencies.

#### Scenario: First-party target with ASAN enabled
- **WHEN** building with `ENABLE_ASAN=ON`
- **AND** the target is a first-party Goggles library or executable
- **THEN** the target is compiled with `-fsanitize=address -fno-omit-frame-pointer`
- **AND** the target is linked with `-fsanitize=address`

#### Scenario: Third-party dependency with ASAN enabled
- **WHEN** building with `ENABLE_ASAN=ON`
- **AND** the target is a third-party dependency (CPM or Conan)
- **THEN** the target is NOT compiled with sanitizer flags
- **AND** the target is NOT linked with sanitizer flags

### Requirement: Runtime Leak Suppressions

The build system SHALL configure LSAN suppressions via external file rather than compiled-in code, enabling transparent suppression of third-party library leaks while detecting first-party leaks.

#### Scenario: CTest runs with LSAN suppressions
- **WHEN** running tests via CTest with `ENABLE_ASAN=ON`
- **THEN** `LSAN_OPTIONS` environment variable includes `suppressions=<path>/lsan.supp`
- **AND** leaks originating entirely within suppressed patterns are not reported
- **AND** leaks in first-party code ARE reported even if called via third-party APIs

