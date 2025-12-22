cmake_minimum_required(VERSION 3.20)
project(goggles VERSION 0.1.0 LANGUAGES CXX)

# Project-wide versioning and naming
add_compile_definitions(GOGGLES_VERSION="${PROJECT_VERSION}")
add_compile_definitions(GOGGLES_PROJECT_NAME="Goggles")

# Layer-only build option (for multiarch layer builds)
option(GOGGLES_LAYER_ONLY "Build only the Vulkan capture layer" OFF)

# Load modular configuration
include(cmake/Dependencies.cmake)
include(cmake/CompilerConfig.cmake)
include(cmake/CodeQuality.cmake)

# Project structure
if(GOGGLES_LAYER_ONLY)
    add_subdirectory(src/capture)
else()
    add_subdirectory(src)

    enable_testing()
    add_subdirectory(tests)
endif()
