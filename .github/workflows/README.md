# GitHub Actions CI Configuration

This directory contains the GitHub Actions CI workflow for the Goggles project.

## Pipeline (`ci.yml`)

Runs on every push/PR to `main` using Pixi tasks and shared presets.

### Jobs

1) **Auto Format**
- `pixi run clang-format -i $(git ls-files '*.cpp' '*.hpp')`
- Commits and pushes formatted code automatically with message `"style: auto-format code"` when changes exist (skips auto-commit on forked PRs).

2) **Build and Test (test preset)**
- Uses Vulkan SDK from the Pixi environment (`vulkansdk` package), no system install needed.
- `pixi run build test` → CMake `test` preset (Debug + ASAN + clang-tidy enabled) via Pixi task.
- `pixi run test test` → Runs tests with AddressSanitizer.

3) **Static Analysis (clang-tidy)**
- `pixi run build quality` → CMake `quality` preset (Debug + ASAN + clang-tidy).

### Reproducing Locally
```bash
# Format
pixi run clang-format -i $(git ls-files '*.cpp' '*.hpp')

# Build + test with ASAN
pixi run build test
pixi run test test

# Static analysis
pixi run build quality
```

### Debugging CI Failures
- **Format commit missing**: Ensure files are tracked; rerun clang-format command above.
- **Build/Test failures**: `pixi run build test && pixi run test test` locally to match CI flags.
- **Clang-tidy issues**: `pixi run build quality` or `pixi run clang-tidy -- <file>` with compile commands from `build/quality`.
