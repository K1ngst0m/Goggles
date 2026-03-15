set(FC_TEST_SOURCES
    contract/test_filter_chain.cpp
    contract/test_filter_controls.cpp
    contract/test_preset_parser.cpp
    contract/test_retroarch_preprocessor.cpp
    contract/test_slang_reflect.cpp
    contract/test_shader_runtime.cpp
    contract/test_semantic_binder.cpp
    contract/test_zfast_integration.cpp
    contract/test_shader_validation.cpp
    contract/test_diagnostic_event_model.cpp
    contract/test_diagnostic_sinks.cpp
    contract/test_binding_ledger.cpp
    contract/test_diagnostic_ledgers.cpp
    contract/test_diagnostic_session.cpp
    contract/test_diagnostic_reporting.cpp
    contract/test_source_provenance.cpp
    contract/test_compile_report.cpp
    contract/test_authoring_validation.cpp
    contract/test_gpu_timestamp_pool.cpp
    contract/test_shader_batch_report.cpp
    contract/shader_batch_report.cpp
    contract/test_runtime_diagnostics.cpp
    contract/test_filter_chain_c_api_contracts.cpp
)

add_executable(fc_tests ${FC_TEST_SOURCES})

target_include_directories(fc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_compile_definitions(fc_tests PRIVATE
    FILTER_CHAIN_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets"
)
target_link_libraries(fc_tests PRIVATE
    goggles-filter-chain
    Catch2::Catch2WithMain
)
add_test(NAME fc_contract_tests COMMAND fc_tests)

add_executable(fc_shader_batch_report
    contract/shader_batch_report_main.cpp
    contract/shader_batch_report.cpp
)
target_include_directories(fc_shader_batch_report PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/contract
)
target_compile_definitions(fc_shader_batch_report PRIVATE
    FILTER_CHAIN_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets"
)
target_link_libraries(fc_shader_batch_report PRIVATE
    goggles-filter-chain
)
