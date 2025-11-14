cmake_minimum_required(VERSION 3.20)
project(goggles VERSION 0.1.0 LANGUAGES CXX)

# Load modular configuration
include(cmake/Dependencies.cmake)
include(cmake/CompilerConfig.cmake)
include(cmake/CodeQuality.cmake)

# Project structure
add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
