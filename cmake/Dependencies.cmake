# ============================================================================
# Dependencies Management
# ============================================================================
# Most dependencies are provided by Pixi (see pixi.toml)
# CPM is not used; profiling Tracy is provided by Pixi
# ============================================================================

# ============================================================================
# Profiling Dependencies (loaded early for layer-only builds)
# ============================================================================

if(ENABLE_PROFILING)
    # Tracy must be provided by Pixi
    find_package(Tracy REQUIRED CONFIG)

    # Ensure Tracy is built with PIC for linking into shared libraries
    if(TARGET Tracy::TracyClient)
        set_target_properties(Tracy::TracyClient PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endif()

# Layer-only builds skip most dependencies
if(GOGGLES_LAYER_ONLY)
    find_package(Vulkan REQUIRED)
    return()
endif()

# ============================================================================
# Core Dependencies (provided by Pixi)
# ============================================================================

find_package(Threads REQUIRED)

# expected-lite is provided by Pixi
find_package(expected-lite REQUIRED)

find_package(spdlog REQUIRED)

find_package(toml11 REQUIRED)

find_package(Catch2 REQUIRED)

find_package(CLI11 REQUIRED)

# Verify Pixi environment for header-only libraries that use CONDA_PREFIX
if(NOT DEFINED ENV{CONDA_PREFIX})
    message(FATAL_ERROR
        "CONDA_PREFIX environment variable not set.\n"
        "This project must be built within a Pixi environment.\n"
        "Run: pixi run build [preset]")
endif()

add_library(stb_image INTERFACE)
target_include_directories(stb_image SYSTEM INTERFACE $ENV{CONDA_PREFIX}/include/stb)

add_library(BS_thread_pool INTERFACE)
target_include_directories(BS_thread_pool SYSTEM INTERFACE $ENV{CONDA_PREFIX}/include)

find_package(SDL3 REQUIRED)

find_package(slang REQUIRED CONFIG)

find_package(Vulkan REQUIRED)
