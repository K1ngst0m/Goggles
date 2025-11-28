# ============================================================================
# Code Quality Tools
# ============================================================================

option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        # Store clang-tidy configuration for project targets only
        set(GOGGLES_CLANG_TIDY_CONFIG
            ${CLANG_TIDY_EXE};
            --header-filter=${CMAKE_SOURCE_DIR}/src/.*;
        )
        
        # Helper function to apply clang-tidy to project targets
        function(goggles_enable_clang_tidy target_name)
            set_target_properties(${target_name} PROPERTIES
                CXX_CLANG_TIDY "${GOGGLES_CLANG_TIDY_CONFIG}"
            )
        endfunction()
        
        message(STATUS "Clang-tidy enabled: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "ENABLE_CLANG_TIDY=ON but clang-tidy not found")
        set(ENABLE_CLANG_TIDY OFF CACHE BOOL "Clang-tidy not available" FORCE)
    endif()
else()
    function(goggles_enable_clang_tidy target_name)
        # No-op when clang-tidy is disabled
    endfunction()
endif()
