# Render shader runtime

add_library(goggles_render_shader_obj OBJECT
    shader_runtime.cpp
    retroarch_preprocessor.cpp
    slang_reflect.cpp
)

target_include_directories(goggles_render_shader_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(goggles_render_shader_obj PRIVATE
    GOGGLES_LOG_TAG="render.shader"
)

target_link_libraries(goggles_render_shader_obj PUBLIC
    goggles_util
    Vulkan::Vulkan
    SDL3::SDL3
    slang::slang
    stb_image
)

target_compile_definitions(goggles_render_shader_obj PUBLIC
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

goggles_enable_clang_tidy(goggles_render_shader_obj)
goggles_enable_sanitizers(goggles_render_shader_obj)
goggles_enable_profiling(goggles_render_shader_obj)
