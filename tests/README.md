# Goggles Test Suite

This directory contains comprehensive tests for the Goggles project utility modules using Catch2 v3.

## Test Coverage

- **Error Handling** (`test_error.cpp`): 19 tests covering `tl::expected` usage, error codes, and monadic operations
- **Configuration** (`test_config.cpp`): 15 tests covering TOML parsing, validation, and edge cases  
- **Logging** (`test_logging.cpp`): 9 tests covering logger initialization and functionality

**Total**: 27 test cases with 135+ assertions

**CI Integration**: All tests run automatically on every push/PR using the same `test` preset recommended for local development, ensuring developer-CI consistency with AddressSanitizer enabled.

## Running Tests

### Method 1: Test Preset (Recommended)

**New test-optimized preset with built-in best practices:**

```bash
# From project root - includes Debug + ASAN + parallel builds
cmake --preset test && cmake --build --preset test
ctest --test-dir build/test --output-on-failure

# Or use the integrated CTest preset (CMake 3.20+)
cmake --preset test && cmake --build --preset test
ctest --preset test
```

### Method 2: Via Individual Presets

```bash
# Standard debug build 
cmake --preset debug && cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure

# AddressSanitizer build (for memory debugging)
cmake --preset asan && cmake --build --preset asan
ctest --test-dir build/asan --output-on-failure
```

### Method 3: Individual Test Executable (For Debugging)

```bash
# Navigate to test directory first
cd build/debug/tests

# Run all tests
./goggles_tests

# Run specific tests
./goggles_tests "[config]"
./goggles_tests "[error]" 
./goggles_tests "[logging]"

# Verbose output for debugging
./goggles_tests --success
```

**Important**: Individual test execution requires running from `build/debug/tests/` directory to find test data files.

## Test Data Files

Configuration tests use sample TOML files in `util/test_data/`:
- `valid_config.toml` - Well-formed configuration
- `invalid_config.toml` - Invalid values for validation testing
- `partial_config.toml` - Minimal config using defaults
- `malformed_config.toml` - TOML parse error testing

## Adding New Tests

1. **Follow structure**: Mirror `src/` directory layout in `tests/`
2. **Use Catch2 conventions**: Descriptive test names, sections for organization
3. **Test both paths**: Success and failure cases with `tl::expected`
4. **Follow policies**: See `docs/PROJECT_POLICIES.md` for testing guidelines

Example:
```cpp
TEST_CASE("Module handles edge case correctly", "[module]") {
    SECTION("Success case") {
        auto result = some_function(valid_input);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == expected_value);
    }
    
    SECTION("Error case") {
        auto result = some_function(invalid_input);
        REQUIRE(!result.has_value());
        REQUIRE(result.error().code == ErrorCode::expected_error);
    }
}
```
