find_package(Vulkan REQUIRED)
find_package(expected-lite CONFIG REQUIRED
    HINTS
        "$ENV{CONDA_PREFIX}"
        "$ENV{CONDA_PREFIX}/lib/cmake/expected-lite")

if(NOT DEFINED GOGGLES_FILTER_CHAIN_FIND_PRIVATE_DEPS)
    set(GOGGLES_FILTER_CHAIN_FIND_PRIVATE_DEPS OFF)
endif()

if(GOGGLES_FILTER_CHAIN_FIND_PRIVATE_DEPS)
    find_package(spdlog REQUIRED)
    find_package(slang CONFIG REQUIRED)

    find_path(STB_IMAGE_INCLUDE_DIR NAMES stb_image.h
        HINTS
            "$ENV{CONDA_PREFIX}/include/stb"
            "$ENV{CONDA_PREFIX}/include"
        REQUIRED)
endif()

if(FILTER_CHAIN_BUILD_TESTS AND GOGGLES_FILTER_CHAIN_FIND_PRIVATE_DEPS)
    find_package(Catch2 REQUIRED)
endif()
