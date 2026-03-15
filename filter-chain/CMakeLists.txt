cmake_minimum_required(VERSION 3.20)
project(GogglesFilterChain VERSION 0.1.0 LANGUAGES CXX)

set(FILTER_CHAIN_LIBRARY_TYPE "STATIC" CACHE STRING "Library type for goggles-filter-chain")
set_property(CACHE FILTER_CHAIN_LIBRARY_TYPE PROPERTY STRINGS STATIC SHARED)
option(FILTER_CHAIN_BUILD_TESTS "Build standalone filter-chain tests" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_PROFILING "Enable Tracy profiler integration" OFF)
option(GOGGLES_DIAGNOSTICS_FORENSIC "Enable forensic diagnostics instrumentation" OFF)

string(TOUPPER "${FILTER_CHAIN_LIBRARY_TYPE}" FILTER_CHAIN_LIBRARY_TYPE_NORMALIZED)
if(NOT FILTER_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "STATIC" AND
   NOT FILTER_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
    message(FATAL_ERROR
        "Invalid FILTER_CHAIN_LIBRARY_TYPE='${FILTER_CHAIN_LIBRARY_TYPE}'. Use STATIC or SHARED.")
endif()

include(cmake/CompilerConfig.cmake)
include(cmake/CodeQuality.cmake)
set(GOGGLES_FILTER_CHAIN_FIND_PRIVATE_DEPS ON)
include(cmake/FilterChainDependencies.cmake)
unset(GOGGLES_FILTER_CHAIN_FIND_PRIVATE_DEPS)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(fc_support_obj OBJECT
    src/support/logging.cpp
)
target_compile_definitions(fc_support_obj PRIVATE
    GOGGLES_LOG_TAG="render.support"
)
target_include_directories(fc_support_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(fc_support_obj PRIVATE
    spdlog::spdlog
)
goggles_enable_pic_if_shared(fc_support_obj)

add_library(fc_diagnostics_obj OBJECT
    src/diagnostics/log_sink.cpp
    src/diagnostics/test_harness_sink.cpp
    src/diagnostics/diagnostic_session.cpp
    src/diagnostics/gpu_timestamp_pool.cpp
)
if(GOGGLES_DIAGNOSTICS_FORENSIC)
    target_compile_definitions(fc_diagnostics_obj PUBLIC GOGGLES_DIAGNOSTICS_FORENSIC)
endif()
target_include_directories(fc_diagnostics_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(fc_diagnostics_obj PRIVATE
    Vulkan::Vulkan
    spdlog::spdlog
)
goggles_enable_pic_if_shared(fc_diagnostics_obj)

add_library(fc_shader_obj OBJECT
    src/shader/shader_runtime.cpp
    src/shader/retroarch_preprocessor.cpp
    src/shader/slang_reflect.cpp
)
target_compile_definitions(fc_shader_obj PRIVATE
    GOGGLES_LOG_TAG="render.shader"
)
target_include_directories(fc_shader_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(fc_shader_obj PRIVATE
    slang::slang
)
goggles_enable_pic_if_shared(fc_shader_obj)

add_library(fc_texture_obj OBJECT
    src/texture/texture_loader.cpp
)
target_compile_definitions(fc_texture_obj PRIVATE
    GOGGLES_LOG_TAG="render.texture"
)
target_include_directories(fc_texture_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(fc_texture_obj SYSTEM PRIVATE ${STB_IMAGE_INCLUDE_DIR})
goggles_enable_pic_if_shared(fc_texture_obj)

# stb_image implementation in a dedicated TU, excluded from clang-tidy
# to avoid false-positive clang-analyzer-unix.Malloc diagnostics inside the header.
add_library(fc_stb_image_obj OBJECT
    src/texture/stb_image_impl.cpp
)
target_include_directories(fc_stb_image_obj SYSTEM PRIVATE ${STB_IMAGE_INCLUDE_DIR})
goggles_enable_pic_if_shared(fc_stb_image_obj)

set(FC_CHAIN_SOURCES
    src/chain/chain_runtime.cpp
    src/chain/chain_builder.cpp
    src/chain/chain_resources.cpp
    src/chain/chain_executor.cpp
    src/chain/chain_controls.cpp
    src/chain/filter_controls.cpp
    src/chain/filter_pass.cpp
    src/chain/framebuffer.cpp
    src/chain/output_pass.cpp
    src/chain/downsample_pass.cpp
    src/chain/preset_parser.cpp
    src/chain/frame_history.cpp
    src/chain/vulkan_dispatch.cpp
)
add_library(fc_chain_obj OBJECT ${FC_CHAIN_SOURCES})
target_compile_definitions(fc_chain_obj PRIVATE
    GOGGLES_LOG_TAG="render.chain"
)
target_include_directories(fc_chain_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(fc_chain_obj SYSTEM PRIVATE ${STB_IMAGE_INCLUDE_DIR})
target_link_libraries(fc_chain_obj PRIVATE
    Vulkan::Vulkan
    slang::slang
)
goggles_enable_pic_if_shared(fc_chain_obj)

# ── Embedded asset generation ────────────────────────────────────────────────
include(cmake/EmbedAssets.cmake)

set(FC_EMBEDDED_ASSETS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_assets_data.cpp")
embed_assets(
    OUTPUT_SOURCE "${FC_EMBEDDED_ASSETS_SOURCE}"
    ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    ASSETS
        "internal/blit.vert:shaders/internal/blit.vert.slang"
        "internal/blit.frag:shaders/internal/blit.frag.slang"
        "internal/downsample.frag:shaders/internal/downsample.frag.slang"
)

add_library(fc_runtime_obj OBJECT
    src/runtime/instance.cpp
    src/runtime/device.cpp
    src/runtime/program.cpp
    src/runtime/chain.cpp
    src/runtime/source_resolver.cpp
    "${FC_EMBEDDED_ASSETS_SOURCE}"
)
target_compile_definitions(fc_runtime_obj PRIVATE
    GOGGLES_LOG_TAG="render.runtime"
)
target_include_directories(fc_runtime_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(fc_runtime_obj PRIVATE
    Vulkan::Vulkan
    spdlog::spdlog
    slang::slang
)
target_include_directories(fc_runtime_obj SYSTEM PRIVATE ${STB_IMAGE_INCLUDE_DIR})
goggles_enable_pic_if_shared(fc_runtime_obj)

add_library(fc_api_obj OBJECT
    src/api/c_api.cpp
    src/api/cpp_wrapper.cpp
)
target_include_directories(fc_api_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(fc_api_obj PRIVATE
    Vulkan::Vulkan
    nonstd::expected-lite
)
if(FILTER_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
    target_compile_definitions(fc_api_obj PRIVATE GOGGLES_FC_BUILD_SHARED)
endif()
goggles_enable_pic_if_shared(fc_api_obj)

foreach(target_name IN ITEMS
        fc_support_obj
        fc_diagnostics_obj
        fc_shader_obj
        fc_texture_obj
        fc_chain_obj
        fc_runtime_obj
        fc_api_obj)
    target_compile_definitions(${target_name} PRIVATE
        VULKAN_HPP_NO_EXCEPTIONS
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    )
    goggles_enable_clang_tidy(${target_name})
    goggles_enable_sanitizers(${target_name})
    goggles_enable_profiling(${target_name})
endforeach()

add_library(goggles-filter-chain ${FILTER_CHAIN_LIBRARY_TYPE_NORMALIZED}
    $<TARGET_OBJECTS:fc_support_obj>
    $<TARGET_OBJECTS:fc_diagnostics_obj>
    $<TARGET_OBJECTS:fc_shader_obj>
    $<TARGET_OBJECTS:fc_texture_obj>
    $<TARGET_OBJECTS:fc_stb_image_obj>
    $<TARGET_OBJECTS:fc_chain_obj>
    $<TARGET_OBJECTS:fc_runtime_obj>
    $<TARGET_OBJECTS:fc_api_obj>
)

target_include_directories(goggles-filter-chain
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(goggles-filter-chain PRIVATE
    $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:GOGGLES_FC_BUILD_SHARED>
)
if(FILTER_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "SHARED")
    target_compile_definitions(goggles-filter-chain INTERFACE
        GOGGLES_FC_USE_SHARED
    )
endif()
target_compile_definitions(goggles-filter-chain PUBLIC
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)
target_link_libraries(goggles-filter-chain
    PUBLIC
        Vulkan::Vulkan
        nonstd::expected-lite
    PRIVATE
        spdlog::spdlog
        slang::slang
)

goggles_enable_clang_tidy(goggles-filter-chain)

if(NOT TARGET GogglesFilterChain::goggles-filter-chain)
    add_library(GogglesFilterChain::goggles-filter-chain ALIAS goggles-filter-chain)
endif()

set(FILTER_CHAIN_LIBRARY_TYPE_IS_STATIC OFF)
if(FILTER_CHAIN_LIBRARY_TYPE_NORMALIZED STREQUAL "STATIC")
    set(FILTER_CHAIN_LIBRARY_TYPE_IS_STATIC ON)
endif()

configure_package_config_file(
    cmake/GogglesFilterChainConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/GogglesFilterChainConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GogglesFilterChain
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/GogglesFilterChainConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(TARGETS goggles-filter-chain EXPORT GogglesFilterChainTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Built-in runtime assets are embedded in the library binary via EmbedAssets.cmake.
# The assets/ directory is no longer installed as a public package artifact.

install(EXPORT GogglesFilterChainTargets
    NAMESPACE GogglesFilterChain::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GogglesFilterChain
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/GogglesFilterChainConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/GogglesFilterChainConfigVersion.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FilterChainDependencies.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GogglesFilterChain
)

if(FILTER_CHAIN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
