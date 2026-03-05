# ============================================================================
# Code Quality Tools
# ============================================================================

option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)

    set(GOGGLES_CLANG_TIDY_CONFIG
        ${CLANG_TIDY_EXE}
        --header-filter=${CMAKE_SOURCE_DIR}/src/.*
        --exclude-header-filter=${CMAKE_SOURCE_DIR}/src/render/chain/api/c/goggles_filter_chain\\.h)

    function(goggles_enable_clang_tidy target_name)
        set_target_properties(${target_name} PROPERTIES CXX_CLANG_TIDY "${GOGGLES_CLANG_TIDY_CONFIG}")
    endfunction()

    message(STATUS "clang-tidy: ${CLANG_TIDY_EXE}")
else()
    function(goggles_enable_clang_tidy target_name)
    endfunction()
endif()
