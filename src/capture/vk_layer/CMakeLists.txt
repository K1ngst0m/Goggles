# Standalone CMakeLists for Vulkan capture layer
# Supports building the layer independently without main project dependencies
#
# Usage:
#   64-bit: cmake -B build64 -S src/capture/vk_layer && cmake --build build64
#   32-bit: cmake -B build32 -S src/capture/vk_layer -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-i686.cmake && cmake --build build32

cmake_minimum_required(VERSION 3.20)
project(goggles_vklayer VERSION 0.1.0 LANGUAGES CXX)

# Determine architecture for output paths and manifest selection
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(GOGGLES_ARCH "x86_64")
    set(GOGGLES_LAYER_NAME "VK_LAYER_goggles_capture_64")
else()
    set(GOGGLES_ARCH "i386")
    set(GOGGLES_LAYER_NAME "VK_LAYER_goggles_capture_32")
endif()

message(STATUS "Building ${GOGGLES_ARCH} layer: ${GOGGLES_LAYER_NAME}")

# Find Vulkan headers (don't need to link the loader)
find_package(Vulkan REQUIRED)

add_library(goggles_vklayer SHARED
    layer_main.cpp
    vk_dispatch.cpp
    vk_hooks.cpp
    vk_capture.cpp
    frame_dump.cpp
    wsi_virtual.cpp
    ipc_socket.cpp
)

target_include_directories(goggles_vklayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../  # For src/ includes
    ${Vulkan_INCLUDE_DIRS}
)

# Layer doesn't need to link Vulkan - gets function pointers via chain
# Only link against libc/libstdc++

target_compile_features(goggles_vklayer PRIVATE cxx_std_20)

set_target_properties(goggles_vklayer PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    PREFIX "lib"
    OUTPUT_NAME "goggles_vklayer"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${GOGGLES_ARCH}
)

# Generate manifest with correct paths
set(GOGGLES_LAYER_LIBRARY_PATH "${CMAKE_INSTALL_PREFIX}/lib/${GOGGLES_ARCH}/libgoggles_vklayer.so")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../config/goggles_layer.json.in
    ${CMAKE_BINARY_DIR}/share/vulkan/implicit_layer.d/goggles_layer_${GOGGLES_ARCH}.json
    @ONLY
)

# Install rules
install(TARGETS goggles_vklayer
    LIBRARY DESTINATION lib/${GOGGLES_ARCH}
)

install(FILES ${CMAKE_BINARY_DIR}/share/vulkan/implicit_layer.d/goggles_layer_${GOGGLES_ARCH}.json
    DESTINATION share/vulkan/implicit_layer.d
)
