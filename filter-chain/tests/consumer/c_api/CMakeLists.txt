cmake_minimum_required(VERSION 3.20)
project(FCCApiConsumer LANGUAGES C)

find_package(GogglesFilterChain CONFIG REQUIRED)

add_executable(c_api_consumer main.c)
target_link_libraries(c_api_consumer PRIVATE GogglesFilterChain::goggles-filter-chain)
target_compile_features(c_api_consumer PRIVATE c_std_11)

enable_testing()
add_test(NAME c_api_consumer_test COMMAND c_api_consumer)
