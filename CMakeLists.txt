cmake_minimum_required(VERSION 3.20)
project(goggles VERSION 0.1.0 LANGUAGES CXX)

# ============================================================================
# Package Management Setup
# ============================================================================

# CPM.cmake for simple/header-only dependencies
include(cmake/CPM.cmake)

# Use a global CPM source cache to speed up builds
set(CPM_SOURCE_CACHE $ENV{HOME}/.cache/CPM CACHE PATH "CPM source cache directory")

# Simple dependencies via CPM
# Note: Versions are pinned for reproducibility
CPMAddPackage(
    NAME expected-lite
    GITHUB_REPOSITORY martinmoene/expected-lite
    VERSION 0.8.0
    OPTIONS
        "EXPECTED_LITE_OPT_BUILD_TESTS OFF"
        "EXPECTED_LITE_OPT_BUILD_EXAMPLES OFF"
)

CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.15.0
    OPTIONS
        "SPDLOG_BUILD_SHARED OFF"
        "SPDLOG_BUILD_EXAMPLE OFF"
        "SPDLOG_BUILD_TESTS OFF"
)

CPMAddPackage(
    NAME toml11
    GITHUB_REPOSITORY ToruNiina/toml11
    VERSION 4.2.0
    OPTIONS
        "toml11_BUILD_TESTS OFF"
)

CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    VERSION 3.8.0
    OPTIONS
        "CATCH_BUILD_TESTING OFF"
        "CATCH_INSTALL_DOCS OFF"
        "CATCH_INSTALL_EXTRAS OFF"
)

# Threading & Concurrency dependencies
# Uncomment when implementing multi-threading (see docs/threading_architecture.md)

# BS::thread_pool - Simple job submission
# CPMAddPackage(
#     NAME BS_thread_pool
#     GITHUB_REPOSITORY bshoshany/thread-pool
#     VERSION 3.5.0
#     OPTIONS
#         "BUILD_TESTS OFF"
#         "BUILD_EXAMPLES OFF"
# )

# rigtorp::SPSCQueue - Wait-free queue for real-time IPC
# CPMAddPackage(
#     NAME rigtorp_SPSCQueue
#     GITHUB_REPOSITORY rigtorp/SPSCQueue
#     VERSION 1.2.0
# )

# Taskflow - Dependency-aware task scheduling (upgrade path)
# CPMAddPackage(
#     NAME taskflow
#     GITHUB_REPOSITORY taskflow/taskflow
#     VERSION 3.10.0
#     OPTIONS
#         "TF_BUILD_EXAMPLES OFF"
#         "TF_BUILD_TESTS OFF"
# )

# Complex dependencies via vcpkg (slang)
# These are found via find_package() when CMAKE_TOOLCHAIN_FILE is set to vcpkg
# find_package(slang CONFIG REQUIRED)
# Uncommented when actually needed in Prototype 5+

# System dependencies
# find_package(Vulkan REQUIRED)
# Uncommented when needed in Prototype 1

# ============================================================================
# Build Configuration
# ============================================================================

# Enforce C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Strict warning flags
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
)

# Optional sanitizer toggles
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

# Add subdirectories
add_subdirectory(src)

# Enable testing
enable_testing()
add_subdirectory(tests)
