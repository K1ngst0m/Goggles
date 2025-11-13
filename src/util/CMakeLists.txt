# Utility subsystem
# Provides error handling, logging, and configuration infrastructure

add_library(goggles_util STATIC
    logging.cpp
    config.cpp
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
