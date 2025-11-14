# Capture subsystem

# Vulkan layer (shared library stub)
add_library(goggles_vklayer SHARED
    vk_layer/layer_stub.cpp
)

target_include_directories(goggles_vklayer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Set library properties
set_target_properties(goggles_vklayer PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    PREFIX ""
)

# Capture backends library (stub)
add_library(goggles_capture INTERFACE)

target_include_directories(goggles_capture INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

goggles_enable_clang_tidy(goggles_vklayer)
