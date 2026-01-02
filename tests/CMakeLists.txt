# Test configuration for Goggles project
# Uses Catch2 v3 as testing framework per PROJECT_POLICIES.md
#
# EXECUTION METHODS:
# 1. Via CTest (recommended): ctest --test-dir build/debug --output-on-failure
# 2. Individual debugging: cd build/debug/tests && ./goggles_tests
# 
# Note: Test data files are copied to build/tests/util/test_data/ and tests
# expect to run from that directory to find test data files.

# Test executable
add_executable(goggles_tests)

# Test sources - mirrors src/ structure
target_sources(goggles_tests PRIVATE
    # Utility module tests
    util/test_error.cpp
    util/test_config.cpp
    util/test_logging.cpp
    util/test_job_system.cpp
    util/test_queues.cpp
    util/test_unique_fd.cpp
    util/test_serializer.cpp

    # Render module tests
    render/test_shader_runtime.cpp
    render/test_retroarch_preprocessor.cpp
    render/test_preset_parser.cpp
    render/test_slang_reflect.cpp
    render/test_semantic_binder.cpp
    render/test_zfast_integration.cpp
    render/test_filter_chain.cpp
    render/test_shader_validation.cpp

    # Capture module tests
    capture/test_capture_protocol.cpp
    capture/test_capture_receiver.cpp

    # Future: Pipeline module tests (when implemented)
    # pipeline/graph/test_pipeline_graph.cpp
)

# Include directories for accessing project headers
target_include_directories(goggles_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Link against project libraries
target_link_libraries(goggles_tests PRIVATE
    goggles_util
    goggles_render
    goggles_capture
    Catch2::Catch2WithMain
)

# Compiler settings (inherit from main project)
target_compile_features(goggles_tests PRIVATE cxx_std_20)

# Define source directory for test asset paths
target_compile_definitions(goggles_tests PRIVATE
    GOGGLES_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

goggles_enable_sanitizers(goggles_tests)

# Copy test data files to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/util/test_data/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/util/test_data/)

# Register test with CTest
add_test(NAME goggles_unit_tests COMMAND goggles_tests)

# Set working directory for test execution to ensure test data files are found
# This allows both CTest execution and individual test executable debugging
set_property(TEST goggles_unit_tests 
    PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Discover Catch2 tests for individual reporting (optional, fallback to simple registration)
# Note: catch_discover_tests may not be available in all Catch2 versions
if(TARGET Catch2::Catch2)
    # Try to find the Catch2 cmake module
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    find_file(CATCH_CMAKE_MODULE "Catch.cmake" PATHS "${catch2_SOURCE_DIR}/extras" "${catch2_SOURCE_DIR}/contrib" NO_DEFAULT_PATH)
    if(CATCH_CMAKE_MODULE)
        include("${CATCH_CMAKE_MODULE}")
        catch_discover_tests(goggles_tests)
    endif()
endif()