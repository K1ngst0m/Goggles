# Utility subsystem
# Provides error handling, logging, and configuration infrastructure

add_library(goggles_util STATIC
    logging.cpp
    config.cpp
    job_system.cpp
)

target_include_directories(goggles_util PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependencies (provided by Pixi)
target_link_libraries(goggles_util PUBLIC
    nonstd::expected-lite
    spdlog::spdlog
    toml11::toml11
    BS_thread_pool
)

goggles_enable_clang_tidy(goggles_util)
goggles_enable_sanitizers(goggles_util)
goggles_enable_profiling(goggles_util)
