# ============================================================================
# Dependencies Management
# ============================================================================

include(cmake/CPM.cmake)

# Use a global CPM source cache to speed up builds
set(CPM_SOURCE_CACHE $ENV{HOME}/.cache/CPM CACHE PATH "CPM source cache directory")

# ============================================================================
# Core Dependencies
# ============================================================================

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
        "SPDLOG_USE_STD_FORMAT ON"
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

# ============================================================================
# Threading & Concurrency Dependencies
# ============================================================================

CPMAddPackage(
    NAME BS_thread_pool
    GITHUB_REPOSITORY bshoshany/thread-pool
    VERSION 3.5.0
    OPTIONS
        "BUILD_TESTS OFF"
        "BUILD_EXAMPLES OFF"
)

# Taskflow - Dependency-aware task scheduling (upgrade path)
# CPMAddPackage(
#     NAME taskflow
#     GITHUB_REPOSITORY taskflow/taskflow
#     VERSION 3.10.0
#     OPTIONS
#         "TF_BUILD_EXAMPLES OFF"
#         "TF_BUILD_TESTS OFF"
# )

# ============================================================================
# Windowing & Platform Dependencies
# ============================================================================

CPMAddPackage(
    NAME SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.2.0
    OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_TEST OFF"
        "SDL_TESTS OFF"
        "SDL_EXAMPLES OFF"
        "SDL_VULKAN ON"
        "SDL_WAYLAND ON"
        "SDL_X11 ON"
        "SDL_PIPEWIRE OFF"
)

# ============================================================================
# Shader Compilation Dependencies
# ============================================================================

set(SLANG_VERSION "2025.23.2")

# Always use release binary (debug-info tarball is separate debug symbols only)
set(SLANG_ARCHIVE "slang-${SLANG_VERSION}-linux-x86_64.tar.gz")
set(SLANG_URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${SLANG_ARCHIVE}")

CPMAddPackage(
    NAME slang
    URL ${SLANG_URL}
    DOWNLOAD_ONLY YES
)

if(slang_ADDED)
    set(SLANG_ROOT "${slang_SOURCE_DIR}")

    # Use the bundled cmake config
    list(APPEND CMAKE_PREFIX_PATH "${SLANG_ROOT}/lib/cmake/slang")
    find_package(slang REQUIRED CONFIG)

    message(STATUS "Slang ${SLANG_VERSION} configured from: ${SLANG_ROOT}")
endif()

# ============================================================================
# System Dependencies
# ============================================================================

find_package(Vulkan REQUIRED)
