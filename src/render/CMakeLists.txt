# Render subsystem

add_library(goggles_render STATIC
    vulkan/vulkan_backend.cpp
)

target_include_directories(goggles_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(goggles_render PUBLIC
    goggles_util
    Vulkan::Vulkan
    SDL3::SDL3-static
)

goggles_enable_clang_tidy(goggles_render)
goggles_enable_sanitizers(goggles_render)
