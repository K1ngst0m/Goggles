# Change: Add Shader Validation & Testing

## Why

Per ROADMAP.md Phase 1, we need to prevent regressions in the filter chain when adding new features. With 1870+ RetroArch shader presets, manual testing is impractical.

## What

Implement the Shader Validation & Testing items from ROADMAP.md:

1. Validate shader compilation for all implemented presets
2. Catch SPIR-V compilation errors early
3. Report shader compilation failures with diagnostics
4. Automated test runner for shader validation
5. Integration with existing test infrastructure (Catch2)

Future (out of scope for this change):
- Golden image generation for reference outputs
- Comparison against golden images
- Automated regression detection

## Scope

### In Scope
- Test harness binary for batch shader validation
- Shell script wrapper (`scripts/test_shaders.sh`)
- JSON output for CI integration
- Catch2 integration for preset parsing tests
- Diagnostic output on compilation failures

### Out of Scope
- GPU rendering / visual regression testing
- Golden image infrastructure
- Performance benchmarking

## Success Criteria

1. `scripts/test_shaders.sh` tests all presets
2. Results in `build/shader_test_results.json`
3. CI integration with regression detection
4. Per-category filtering (e.g., `test_shaders.sh crt`)