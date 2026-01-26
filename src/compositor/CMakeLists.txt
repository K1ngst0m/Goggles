# Compositor subsystem

add_library(goggles_compositor STATIC
    compositor_server.cpp
)

target_include_directories(goggles_compositor
    PUBLIC ${CMAKE_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/protocol
)

target_link_libraries(goggles_compositor
    PUBLIC goggles_util
           SDL3::SDL3
    PRIVATE PkgConfig::wlroots
            PkgConfig::wayland-server
            PkgConfig::xkbcommon
)

# wlroots requires this define to access unstable API
target_compile_definitions(goggles_compositor PRIVATE
    GOGGLES_LOG_TAG="compositor"
    WLR_USE_UNSTABLE
)

goggles_enable_clang_tidy(goggles_compositor)
goggles_enable_sanitizers(goggles_compositor)
goggles_enable_profiling(goggles_compositor)
