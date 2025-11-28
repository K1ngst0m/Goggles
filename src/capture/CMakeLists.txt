# Capture subsystem

# Vulkan layer (shared library)
add_library(goggles_vklayer SHARED
    vk_layer/layer_main.cpp
    vk_layer/vk_dispatch.cpp
    vk_layer/vk_hooks.cpp
    vk_layer/vk_capture.cpp
    vk_layer/ipc_socket.cpp
)

target_include_directories(goggles_vklayer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vk_layer
)

target_link_libraries(goggles_vklayer PRIVATE
    Vulkan::Vulkan
)

# Vulkan layer needs C++20
target_compile_features(goggles_vklayer PRIVATE cxx_std_20)

# Set library properties for layer
set_target_properties(goggles_vklayer PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    PREFIX "lib"
    OUTPUT_NAME "goggles_vklayer"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Copy layer manifest to build directory
configure_file(
    ${CMAKE_SOURCE_DIR}/config/goggles_layer.json
    ${CMAKE_BINARY_DIR}/goggles_layer.json
    COPYONLY
)

# Capture backends library (stub)
add_library(goggles_capture INTERFACE)

target_include_directories(goggles_capture INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

goggles_enable_clang_tidy(goggles_vklayer)
goggles_enable_sanitizers(goggles_vklayer)
