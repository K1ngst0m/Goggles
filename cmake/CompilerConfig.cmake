# ============================================================================
# Compiler Configuration
# ============================================================================

# Enforce C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE and tooling support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Compiler Caching (ccache)
# ============================================================================

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

# ============================================================================
# Warning Configuration
# ============================================================================

add_compile_options(
    -Wall
    -Wextra
    -Wshadow
    -Wconversion
)

# ============================================================================
# Sanitizer Configuration
# ============================================================================

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

add_library(goggles_sanitizer_options INTERFACE)

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    target_compile_options(goggles_sanitizer_options INTERFACE
        -fsanitize=address
        -fno-omit-frame-pointer
    )
    target_link_options(goggles_sanitizer_options INTERFACE
        -fsanitize=address
    )
endif()

if(ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    target_compile_options(goggles_sanitizer_options INTERFACE
        -fsanitize=undefined
    )
    target_link_options(goggles_sanitizer_options INTERFACE
        -fsanitize=undefined
    )
endif()

# Helper function to enable sanitizers on a target
function(goggles_enable_sanitizers target)
    target_link_libraries(${target} PRIVATE goggles_sanitizer_options)
endfunction()
