# Render subsystem

add_library(goggles_render STATIC
    backend/vulkan_backend.cpp
    backend/vulkan_debug.cpp
    shader/shader_runtime.cpp
    shader/retroarch_preprocessor.cpp
    shader/slang_reflect.cpp
    chain/output_pass.cpp
    chain/preset_parser.cpp
    chain/filter_pass.cpp
    chain/framebuffer.cpp
    chain/filter_chain.cpp
    texture/texture_loader.cpp
)

target_include_directories(goggles_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(goggles_render PUBLIC
    goggles_util
    Vulkan::Vulkan
    SDL3::SDL3-static
    slang::slang
    stb_image
)

target_compile_definitions(goggles_render PUBLIC
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

goggles_enable_clang_tidy(goggles_render)
goggles_enable_sanitizers(goggles_render)
