# Render subsystem

add_subdirectory(backend)
add_subdirectory(chain)
add_subdirectory(shader)
add_subdirectory(texture)

set(GOGGLES_CHAIN_LIBRARY_TYPE "STATIC" CACHE STRING
    "Library type for goggles-filter-chain (STATIC or SHARED)")
set_property(CACHE GOGGLES_CHAIN_LIBRARY_TYPE PROPERTY STRINGS STATIC SHARED)

set(GOGGLES_FILTER_UTIL_CORE_TARGET "util_core::util_core" CACHE STRING
    "Target providing util-core API for goggles-filter-chain standalone runtime")

if(NOT TARGET ${GOGGLES_FILTER_UTIL_CORE_TARGET})
    if(GOGGLES_FILTER_UTIL_CORE_TARGET STREQUAL "goggles_util" AND TARGET util_core::util_core)
        set(GOGGLES_FILTER_UTIL_CORE_TARGET "util_core::util_core" CACHE STRING
            "Target providing util-core API for goggles-filter-chain standalone runtime" FORCE)
    else()
        message(FATAL_ERROR
            "Configured util-core target '${GOGGLES_FILTER_UTIL_CORE_TARGET}' is unavailable. "
            "Ensure util-core package wiring is present before configuring render targets.")
    endif()
endif()

string(TOUPPER "${GOGGLES_CHAIN_LIBRARY_TYPE}" GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED)
if(NOT GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "STATIC" AND
   NOT GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
    message(FATAL_ERROR
        "Invalid GOGGLES_CHAIN_LIBRARY_TYPE='${GOGGLES_CHAIN_LIBRARY_TYPE}'. Use STATIC or SHARED.")
endif()

add_library(goggles-filter-chain ${GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED}
    $<TARGET_OBJECTS:goggles_render_chain_obj>
    $<TARGET_OBJECTS:goggles_render_shader_obj>
    $<TARGET_OBJECTS:goggles_render_texture_obj>
)

if(GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
    foreach(runtime_obj_target IN ITEMS
            goggles_render_chain_obj
            goggles_render_shader_obj
            goggles_render_texture_obj)
        set_target_properties(${runtime_obj_target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        target_compile_definitions(${runtime_obj_target} PRIVATE GOGGLES_CHAIN_BUILD_SHARED)
    endforeach()
endif()

target_include_directories(goggles-filter-chain
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/render/chain/api/c>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/render/chain/api/cpp>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(goggles-filter-chain PRIVATE
    $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:GOGGLES_CHAIN_BUILD_SHARED>
)

target_link_libraries(goggles-filter-chain PRIVATE
    Vulkan::Vulkan
    slang::slang
    stb_image
)

target_link_libraries(goggles-filter-chain PUBLIC
    ${GOGGLES_FILTER_UTIL_CORE_TARGET}
)

if(GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
    target_compile_definitions(goggles-filter-chain INTERFACE GOGGLES_CHAIN_USE_SHARED)
endif()

target_compile_definitions(goggles-filter-chain PUBLIC
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

goggles_enable_clang_tidy(goggles-filter-chain)
goggles_enable_sanitizers(goggles-filter-chain)
goggles_enable_profiling(goggles-filter-chain)

add_library(goggles_filter_chain_runtime INTERFACE)

target_link_libraries(goggles_filter_chain_runtime INTERFACE
    goggles-filter-chain
    ${GOGGLES_FILTER_UTIL_CORE_TARGET}
)

target_compile_definitions(goggles_filter_chain_runtime INTERFACE
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)
if(GOGGLES_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
    target_compile_definitions(goggles_filter_chain_runtime INTERFACE GOGGLES_CHAIN_USE_SHARED)
endif()

add_library(goggles_render STATIC
    $<TARGET_OBJECTS:goggles_render_backend_obj>
)

target_include_directories(goggles_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Link deps here so downstream users only need to link goggles_render.
target_link_libraries(goggles_render PUBLIC
    goggles_util
    Vulkan::Vulkan
    SDL3::SDL3
    goggles_filter_chain_runtime
)

target_compile_definitions(goggles_render PUBLIC
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

goggles_enable_clang_tidy(goggles_render)
goggles_enable_sanitizers(goggles_render)
goggles_enable_profiling(goggles_render)

install(FILES
    ${CMAKE_SOURCE_DIR}/src/render/chain/api/c/goggles_filter_chain.h
    ${CMAKE_SOURCE_DIR}/src/render/chain/api/c/goggles_filter_chain_standalone_contract.h
    ${CMAKE_SOURCE_DIR}/src/render/chain/api/cpp/goggles_filter_chain.hpp
    DESTINATION include
)

install(FILES ${CMAKE_SOURCE_DIR}/src/util/error.hpp
    DESTINATION include/util
)

install(TARGETS goggles-filter-chain
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
