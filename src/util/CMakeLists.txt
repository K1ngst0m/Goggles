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

# Link dependencies from CPM
target_link_libraries(goggles_util PUBLIC
    expected-lite
    spdlog::spdlog
    toml11::toml11
)

# Add include directories for header-only threading libraries
target_include_directories(goggles_util PUBLIC
    ${BS_thread_pool_SOURCE_DIR}/include
)

goggles_enable_clang_tidy(goggles_util)
