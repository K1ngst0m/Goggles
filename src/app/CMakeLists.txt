# Main application executable

add_executable(goggles
    main.cpp
)

target_include_directories(goggles PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(goggles PRIVATE
    goggles_util
    goggles_render
    goggles_capture
    SDL3::SDL3
    CLI11::CLI11
)

set_target_properties(goggles PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

goggles_enable_clang_tidy(goggles)
goggles_enable_sanitizers(goggles)
goggles_enable_profiling(goggles)
