# Pipeline subsystem

add_library(goggles_pipeline STATIC
    shader_runtime/shader_runtime.cpp
)

target_include_directories(goggles_pipeline PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(goggles_pipeline PUBLIC
    goggles_util
    slang::slang
)

goggles_enable_clang_tidy(goggles_pipeline)
goggles_enable_sanitizers(goggles_pipeline)
