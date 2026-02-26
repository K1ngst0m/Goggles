# GitHub Actions CI Configuration

This directory contains the GitHub Actions CI workflow for the Goggles project.

## Pipeline (`ci.yml`)

Runs on every push/PR to `main` using Pixi tasks and shared presets.

**Key Optimizations:**
- All jobs run in parallel (no sequential dependencies)
- Merged redundant `build-and-test` + `static-analysis` into single job (both used similar presets)
- Optimized ccache with hendrikmuhs/ccache-action for faster cache restore
- Shared cache keys across jobs for better hit rates
- Larger ccache size (2GB) for improved cache effectiveness

### Jobs

1) **Format Check**
   - Runs in parallel with builds (fail-fast)
   - `pixi run clang-format` and `pixi run taplo fmt`
   - Fails if code needs formatting

2) **Build, Test & Static Analysis** (merged job)
   - Uses Vulkan SDK from the Pixi environment, no system install needed
   - `pixi run build -p test` → CMake `test` preset (Debug + ASAN + clang-tidy + tests)
   - `pixi run test -p test` → Runs tests with AddressSanitizer
   - clang-tidy runs during build (no separate analysis job needed)
   - Test logs uploaded on failure for debugging

3) **Quick Build Check**
   - Fast sanity check with debug preset
   - Ensures compilation works across different configurations

### Expected Performance

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Total Time | ~10 min | ~4-6 min | **40-50% faster** |
| Critical Path | format→build→analysis | max(format, build) | Parallel execution |
| Cache Hit Rate | ~60% | ~80%+ | Shared keys + larger cache |

### Reproducing Locally

```bash
# Format
pixi run clang-format -i $(git ls-files '*.cpp' '*.hpp')

# Build + test with ASAN + clang-tidy
pixi run build -p test
pixi run test -p test

# Quick debug build
pixi run build -p debug
```

### Debugging CI Failures

- **Format failures**: Run `pixi run format` locally
- **Build/Test failures**: `pixi run build -p test && pixi run test -p test`
- **Clang-tidy issues**: Already runs during build, check compile commands in `build/test/compile_commands.json`
- **Test failures**: Download test logs artifact from failed run

## Scheduled (`presets-build.yml`)

Runs weekly (and on manual dispatch) to build every CMake preset, including i686 variants:

```bash
pixi run build-all-presets --clean
```
