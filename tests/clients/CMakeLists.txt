enable_language(C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(wayland-client REQUIRED IMPORTED_TARGET wayland-client)

find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
pkg_get_variable(WL_PROTOCOLS_DIR wayland-protocols pkgdatadir)

set(XDG_SHELL_XML "${WL_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
if(NOT EXISTS "${XDG_SHELL_XML}")
    message(FATAL_ERROR "xdg-shell XML not found at: ${XDG_SHELL_XML}")
endif()
set(XDG_SHELL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
set(XDG_SHELL_CODE "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c")

add_custom_command(
    OUTPUT ${XDG_SHELL_HEADER}
    COMMAND ${WAYLAND_SCANNER} client-header ${XDG_SHELL_XML} ${XDG_SHELL_HEADER}
    DEPENDS ${XDG_SHELL_XML}
)

add_custom_command(
    OUTPUT ${XDG_SHELL_CODE}
    COMMAND ${WAYLAND_SCANNER} private-code ${XDG_SHELL_XML} ${XDG_SHELL_CODE}
    DEPENDS ${XDG_SHELL_XML}
)

add_library(xdg_shell_protocol STATIC ${XDG_SHELL_CODE} ${XDG_SHELL_HEADER})
target_include_directories(xdg_shell_protocol PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(xdg_shell_protocol PUBLIC PkgConfig::wayland-client)

function(add_test_client name)
    cmake_parse_arguments(TC "" "" "SOURCES" ${ARGN})
    add_executable(${name} ${TC_SOURCES})
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE xdg_shell_protocol PkgConfig::wayland-client)
endfunction()

add_test_client(solid_color_client SOURCES solid_color_client.cpp)
add_test_client(quadrant_client SOURCES quadrant_client.cpp)
add_test_client(gradient_client SOURCES gradient_client.cpp)
add_test_client(multi_surface_client SOURCES multi_surface_client.cpp)
