# Utility subsystem
# Provides error handling, logging, and configuration infrastructure

add_library(goggles_util_logging_obj OBJECT
    logging.cpp
)

target_include_directories(goggles_util_logging_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(goggles_util_logging_obj PRIVATE
    goggles_core
    spdlog::spdlog
)

goggles_enable_clang_tidy(goggles_util_logging_obj)
goggles_enable_sanitizers(goggles_util_logging_obj)
goggles_enable_profiling(goggles_util_logging_obj)

add_library(goggles_util STATIC
    $<TARGET_OBJECTS:goggles_util_logging_obj>
    config.cpp
    paths.cpp
    job_system.cpp
)

target_include_directories(goggles_util PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependencies (provided by Pixi)
target_link_libraries(goggles_util PUBLIC
    goggles_core
    spdlog::spdlog
    toml11::toml11
    BS_thread_pool
    Threads::Threads
)

goggles_enable_clang_tidy(goggles_util)
goggles_enable_sanitizers(goggles_util)
goggles_enable_profiling(goggles_util)
