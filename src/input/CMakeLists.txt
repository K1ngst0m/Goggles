# Input forwarding subsystem

add_library(goggles_input STATIC
    input_forwarder.cpp
    compositor_server.cpp
)

target_include_directories(goggles_input
    PUBLIC ${CMAKE_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}  # For xdg-shell-protocol.h
)

target_link_libraries(goggles_input
    PUBLIC goggles_util
    PRIVATE PkgConfig::wlroots
            PkgConfig::wayland-server
            PkgConfig::xkbcommon
)

# wlroots requires this define to access unstable API
target_compile_definitions(goggles_input PRIVATE
    GOGGLES_LOG_TAG="input"
    WLR_USE_UNSTABLE
)

goggles_enable_clang_tidy(goggles_input)
goggles_enable_sanitizers(goggles_input)
goggles_enable_profiling(goggles_input)
