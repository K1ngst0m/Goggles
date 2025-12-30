# Input forwarding subsystem

add_library(goggles_input STATIC
    input_forwarder.cpp
    xwayland_server.cpp
)

target_include_directories(goggles_input
    PUBLIC ${CMAKE_SOURCE_DIR}/src
    PRIVATE ${wlroots_INCLUDE_DIRS}
            ${wayland-server_INCLUDE_DIRS}
            ${xkbcommon_INCLUDE_DIRS}
            ${X11_INCLUDE_DIR}
)

target_link_libraries(goggles_input
    PUBLIC goggles_util
    PRIVATE PkgConfig::wlroots
            PkgConfig::wayland-server
            PkgConfig::xkbcommon
            ${X11_LIBRARIES}
            ${X11_XTest_LIB}
)

# wlroots requires this define to access unstable API
target_compile_definitions(goggles_input PRIVATE WLR_USE_UNSTABLE)

goggles_enable_clang_tidy(goggles_input)
goggles_enable_sanitizers(goggles_input)
goggles_enable_profiling(goggles_input)
