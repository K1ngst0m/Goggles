cmake_minimum_required(VERSION 3.20)
project(goggles VERSION 0.1.0 LANGUAGES CXX)

# Project-wide versioning and naming (single source of truth: project(VERSION ...) above)
add_compile_definitions(GOGGLES_VERSION="${PROJECT_VERSION}")
add_compile_definitions(GOGGLES_VERSION_MAJOR=${PROJECT_VERSION_MAJOR})
add_compile_definitions(GOGGLES_VERSION_MINOR=${PROJECT_VERSION_MINOR})
add_compile_definitions(GOGGLES_VERSION_PATCH=${PROJECT_VERSION_PATCH})
add_compile_definitions(GOGGLES_PROJECT_NAME="Goggles")

# Load modular configuration
include(cmake/Dependencies.cmake)
include(cmake/CompilerConfig.cmake)
include(cmake/CodeQuality.cmake)

# Project structure
add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
