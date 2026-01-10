# Render filter chain

add_library(goggles_render_chain_obj OBJECT
    output_pass.cpp
    preset_parser.cpp
    filter_pass.cpp
    framebuffer.cpp
    filter_chain.cpp
    frame_history.cpp
)

target_include_directories(goggles_render_chain_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(goggles_render_chain_obj PRIVATE
    GOGGLES_LOG_TAG="render.chain"
)

target_link_libraries(goggles_render_chain_obj PUBLIC
    goggles_util
    Vulkan::Vulkan
    SDL3::SDL3
    slang::slang
    stb_image
)

target_compile_definitions(goggles_render_chain_obj PUBLIC
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

goggles_enable_clang_tidy(goggles_render_chain_obj)
goggles_enable_sanitizers(goggles_render_chain_obj)
goggles_enable_profiling(goggles_render_chain_obj)
