add_library(goggles_ui STATIC
    imgui_layer.cpp
)

target_include_directories(goggles_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(goggles_ui PUBLIC
    goggles_util
    imgui
    Vulkan::Vulkan
    SDL3::SDL3
)

target_compile_definitions(goggles_ui PRIVATE
    VULKAN_HPP_NO_EXCEPTIONS
)

goggles_enable_clang_tidy(goggles_ui)
goggles_enable_sanitizers(goggles_ui)