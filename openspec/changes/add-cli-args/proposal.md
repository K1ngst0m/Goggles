# Change: Add CLI Argument Parsing

## Why
The Goggles application previously hardcoded the configuration path and lacked runtime overrides for shader presets. This limited usability for development and testing. 

## What Changes
- **CLI11 Integration**: Added `CLI11` as a modern, declarative command-line parser.
- **Header-Only Module**: Implemented parsing logic in `src/app/cli.hpp` to maintain a clean codebase.
- **Exception Safety**: Encapsulated CLI11's exception-based control flow (help/version/errors) within the library boundary, returning a policy-compliant `Result<CliOptions>` to the application.
- **Key Options**:
    - `--config`, `-c`: Override default configuration path.
    - `--shader`, `-s`: Override shader preset.
    - `--help`, `-h`: Automatic help generation.
    - `--version`, `-v`: Display version info.

## Impact
- **Affected specs**: `app-window`
- **Affected code**:
    - `src/app/main.cpp` (simplified orchestration)
    - `src/app/cli.hpp` (new parsing module)
    - `cmake/Dependencies.cmake` (new dependency)
    - `src/app/CMakeLists.txt` (linkage)