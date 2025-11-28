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

### Requirement: Scoped Clang-Tidy Configuration

The build system SHALL apply clang-tidy static analysis with per-directory configuration, allowing different strictness levels for different code categories.

#### Scenario: Application code with clang-tidy enabled
- **WHEN** building with `ENABLE_CLANG_TIDY=ON`
- **AND** the source file is in `src/app/` or `src/capture/capture_protocol.hpp`
- **THEN** the file is analyzed with the root `.clang-tidy` configuration
- **AND** all checks are enforced with `-warnings-as-errors`

#### Scenario: Vulkan layer code with clang-tidy enabled
- **WHEN** building with `ENABLE_CLANG_TIDY=ON`
- **AND** the source file is in `src/capture/vk_layer/`
- **THEN** the file is analyzed with `src/capture/vk_layer/.clang-tidy`
- **AND** naming and modernization checks are relaxed for Vulkan interop compatibility
- **AND** safety-related checks remain enabled

#### Scenario: Root clang-tidy enforces project conventions
- **WHEN** the root `.clang-tidy` is used
- **THEN** private members MUST use `m_` prefix
- **AND** enum constants MUST use `UPPER_SNAKE_CASE`
- **AND** functions MUST use `snake_case`
- **AND** C-style arrays are forbidden in favor of `std::array`

