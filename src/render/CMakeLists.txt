# Render subsystem

add_subdirectory(backend)
add_subdirectory(chain)
add_subdirectory(shader)
add_subdirectory(texture)

add_library(goggles_render STATIC
    $<TARGET_OBJECTS:goggles_render_backend_obj>
    $<TARGET_OBJECTS:goggles_render_chain_obj>
    $<TARGET_OBJECTS:goggles_render_shader_obj>
    $<TARGET_OBJECTS:goggles_render_texture_obj>
)

target_include_directories(goggles_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Link deps here so downstream users only need to link goggles_render.
target_link_libraries(goggles_render PUBLIC
    goggles_util
    Vulkan::Vulkan
    SDL3::SDL3
    slang::slang
    stb_image
)

target_compile_definitions(goggles_render PUBLIC
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

goggles_enable_clang_tidy(goggles_render)
goggles_enable_sanitizers(goggles_render)
goggles_enable_profiling(goggles_render)
