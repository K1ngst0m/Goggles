# Capture subsystem

# Determine architecture for output paths and manifest
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(GOGGLES_ARCH "x86_64")
    set(GOGGLES_LAYER_NAME "VK_LAYER_goggles_capture_64")
else()
    set(GOGGLES_ARCH "i386")
    set(GOGGLES_LAYER_NAME "VK_LAYER_goggles_capture_32")
endif()

# Vulkan layer (shared library)
add_library(goggles_vklayer SHARED
    vk_layer/layer_main.cpp
    vk_layer/vk_dispatch.cpp
    vk_layer/vk_hooks.cpp
    vk_layer/vk_capture.cpp
    vk_layer/ipc_socket.cpp
    vk_layer/wsi_virtual.cpp
)

target_include_directories(goggles_vklayer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vk_layer
    ${Vulkan_INCLUDE_DIRS}
)

# Layer doesn't need to link Vulkan - gets function pointers via dispatch chain

# Vulkan layer needs C++20
target_compile_features(goggles_vklayer PRIVATE cxx_std_20)

# Set library properties for layer
set_target_properties(goggles_vklayer PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    PREFIX "lib"
    OUTPUT_NAME "goggles_vklayer"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${GOGGLES_ARCH}
)

# Generate layer manifest with correct paths
set(GOGGLES_LAYER_LIBRARY_PATH "${CMAKE_BINARY_DIR}/lib/${GOGGLES_ARCH}/libgoggles_vklayer.so")
configure_file(
    ${CMAKE_SOURCE_DIR}/config/goggles_layer.json.in
    ${CMAKE_BINARY_DIR}/share/vulkan/implicit_layer.d/goggles_layer_${GOGGLES_ARCH}.json
    @ONLY
)

# Capture library (receiver side) - only for full builds
if(NOT GOGGLES_LAYER_ONLY)
    add_library(goggles_capture STATIC capture_receiver.cpp)

    target_include_directories(goggles_capture PUBLIC ${CMAKE_SOURCE_DIR}/src)

    target_link_libraries(goggles_capture PRIVATE
        goggles_util
    )

    goggles_enable_profiling(goggles_capture)
endif()

goggles_enable_clang_tidy(goggles_vklayer)
goggles_enable_sanitizers(goggles_vklayer)
goggles_enable_profiling(goggles_vklayer)
