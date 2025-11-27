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
    -Wpedantic
    -Wshadow
    -Wconversion
)

# ============================================================================
# Sanitizer Configuration
# ============================================================================

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)

    # LSAN suppressions for external libraries (generated at configure time)
    file(WRITE ${CMAKE_BINARY_DIR}/lsan_suppressions.cpp
        "extern \"C\" const char* __lsan_default_suppressions() {\n"
        "    return \"leak:lib\";\n"
        "}\n"
    )
    add_library(lsan_suppressions STATIC ${CMAKE_BINARY_DIR}/lsan_suppressions.cpp)
    link_libraries(lsan_suppressions)
    add_link_options(-Wl,--undefined=__lsan_default_suppressions)
endif()

if(ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()
