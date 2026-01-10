# shader-testing Specification

## Purpose
TBD - created by archiving change add-shader-batch-test. Update Purpose after archive.
## Requirements
### Requirement: Batch shader preset testing
The system SHALL provide a batch testing tool that validates all RetroArch shader presets for parsing and compilation compatibility.

#### Scenario: Run batch test on all presets
Given the shaders/retroarch directory contains .slangp files
When `scripts/test_shaders.sh` is executed
Then each preset is tested for parse and compile success
And results are written to build/shader_test_results.json
And exit code is 0 if no regressions, non-zero otherwise

#### Scenario: Filter by category
Given a category argument is provided (e.g., "crt")
When `scripts/test_shaders.sh crt` is executed
Then only presets under shaders/retroarch/crt/ are tested

### Requirement: Machine-readable results
The batch test tool SHALL output results in JSON format containing per-preset status and summary statistics.

#### Scenario: JSON output format
Given batch tests have completed
When results are written to build/shader_test_results.json
Then each preset entry includes: path, parse_ok, compile_ok, error (if any)
And summary includes: total, passed, failed, skipped counts

