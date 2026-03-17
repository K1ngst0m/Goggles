add_library(stb_image_read_impl OBJECT stb_image_impl.cpp)
target_link_libraries(stb_image_read_impl PRIVATE stb_image)

add_library(stb_image_write_test_impl OBJECT stb_image_write_impl.cpp)
target_link_libraries(stb_image_write_test_impl PRIVATE stb_image)

add_library(image_compare STATIC image_compare.cpp)
target_include_directories(image_compare PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(image_compare PUBLIC
    nonstd::expected-lite
)
target_link_libraries(image_compare PRIVATE
    stb_image
)
target_sources(image_compare PRIVATE
    $<TARGET_OBJECTS:stb_image_read_impl>
    $<TARGET_OBJECTS:stb_image_write_test_impl>
)
target_compile_features(image_compare PRIVATE cxx_std_20)
target_include_directories(image_compare PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(goggles_image_compare image_compare_cli.cpp)
target_link_libraries(goggles_image_compare PRIVATE image_compare)
target_compile_features(goggles_image_compare PRIVATE cxx_std_20)
target_include_directories(goggles_image_compare PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_image_compare test_image_compare.cpp)
target_link_libraries(test_image_compare PRIVATE
    image_compare
    Catch2::Catch2WithMain
)
target_compile_features(test_image_compare PRIVATE cxx_std_20)
target_include_directories(test_image_compare PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME image_compare_unit_tests COMMAND test_image_compare)
set_tests_properties(image_compare_unit_tests PROPERTIES LABELS "unit")
set_property(TEST image_compare_unit_tests PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Visual regression test: aspect ratio geometry
add_executable(test_aspect_ratio test_aspect_ratio.cpp)
target_link_libraries(test_aspect_ratio PRIVATE
    image_compare
    Catch2::Catch2WithMain
)
target_compile_features(test_aspect_ratio PRIVATE cxx_std_20)
target_include_directories(test_aspect_ratio PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_aspect_ratio PRIVATE
    GOGGLES_BINARY="$<TARGET_FILE:goggles>"
    QUADRANT_CLIENT_BINARY="$<TARGET_FILE:quadrant_client>"
    VISUAL_CONFIGS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/configs"
)

add_test(NAME test_aspect_ratio COMMAND test_aspect_ratio)
set_tests_properties(test_aspect_ratio PROPERTIES
    LABELS "visual"
    TIMEOUT 120
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Visual regression test: shader bypass and zfast-crt
add_executable(test_shader_basic test_shader_basic.cpp)
target_link_libraries(test_shader_basic PRIVATE
    image_compare
    Catch2::Catch2WithMain
)
target_compile_features(test_shader_basic PRIVATE cxx_std_20)
target_include_directories(test_shader_basic PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_shader_basic PRIVATE
    GOGGLES_BINARY="$<TARGET_FILE:goggles>"
    QUADRANT_CLIENT_BINARY="$<TARGET_FILE:quadrant_client>"
    VISUAL_CONFIGS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/configs"
    GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden"
)

add_test(NAME test_shader_basic COMMAND test_shader_basic)
set_tests_properties(test_shader_basic PROPERTIES
    LABELS "visual"
    TIMEOUT 120
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
