# Main application executable

add_executable(goggles
    application.cpp
    cli.cpp
    main.cpp
    sdl_platform.cpp
    ui_controller.cpp
)

target_include_directories(goggles PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(goggles PRIVATE
    goggles_util
    goggles_render
    goggles_capture
    goggles_ui
    SDL3::SDL3
    goggles_input
    CLI11::CLI11
)

target_compile_definitions(goggles PRIVATE
    GOGGLES_LOG_TAG="app"
)

set_target_properties(goggles PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

goggles_enable_clang_tidy(goggles)
goggles_enable_sanitizers(goggles)
goggles_enable_profiling(goggles)

# Reaper watchdog process
add_executable(goggles-reaper reaper_main.cpp)
set_target_properties(goggles-reaper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
goggles_enable_clang_tidy(goggles-reaper)
