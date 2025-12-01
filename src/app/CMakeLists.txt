# Main application executable

add_executable(goggles
    main.cpp
    capture_receiver.cpp
)

target_include_directories(goggles PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(goggles PRIVATE
    goggles_util
    goggles_render
    SDL3::SDL3-static
)

goggles_enable_clang_tidy(goggles)
goggles_enable_sanitizers(goggles)
