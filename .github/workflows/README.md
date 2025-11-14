# GitHub Actions CI Configuration

This directory contains the GitHub Actions CI workflow for the Goggles project.

## Current CI Pipeline (`ci.yml`)

### Overview
The CI pipeline runs on every push and pull request to the `main` branch, providing comprehensive code quality and correctness validation.

### Pipeline Steps

1. **Configure with Test Preset**
   - Uses `cmake --preset test` (Debug + AddressSanitizer + parallel builds)
   - Ensures identical configuration to local testing workflow

2. **Build with Test Preset**  
   - Uses `cmake --build --preset test`
   - Parallel compilation for faster CI execution
   - Memory safety checking enabled via ASAN

3. **Run Comprehensive Test Suite**
   - Uses `ctest --preset test` 
   - Executes all 27 test cases (135+ assertions)
   - Verbose output for debugging failures
   - Catches memory issues with AddressSanitizer

4. **Code Format Validation**
   - Checks all C++ files with `clang-format`
   - Fails if formatting is inconsistent

5. **Static Analysis**
   - Runs `clang-tidy` on all source files
   - Uses compile database from test build

### Local Testing
To reproduce CI results locally:
```bash
cmake --preset test
cmake --build --preset test  
ctest --preset test
```

### Debugging CI Failures
1. **Test failures**: Run `ctest --preset test` locally with ASAN enabled
2. **Build failures**: Check that code compiles with test preset locally
3. **Format issues**: Run `clang-format -i` on affected files
4. **Static analysis**: Run `clang-tidy -p build/test <file>`
