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

set(
  GOGGLES_FILTER_CHAIN_SOURCE_DIR
  "${CMAKE_SOURCE_DIR}/filter-chain"
  CACHE PATH "Path to the goggles-filter-chain checkout or submodule"
)

if(NOT EXISTS "${GOGGLES_FILTER_CHAIN_SOURCE_DIR}/CMakeLists.txt")
  message(
    FATAL_ERROR
      "GOGGLES_FILTER_CHAIN_SOURCE_DIR must point to a goggles-filter-chain checkout "
      "containing CMakeLists.txt (current: ${GOGGLES_FILTER_CHAIN_SOURCE_DIR})"
  )
endif()

add_subdirectory("${GOGGLES_FILTER_CHAIN_SOURCE_DIR}" filter-chain)

# Project structure
add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
