# EmbedAssets.cmake
# Generates a C++ source file containing embedded shader assets as byte arrays.
#
# Usage:
#   embed_assets(
#       OUTPUT_SOURCE <output.cpp>
#       ASSET_DIR <dir>
#       ASSETS <id1:relative_path1> <id2:relative_path2> ...
#   )

function(embed_assets)
    cmake_parse_arguments(EA "" "OUTPUT_SOURCE;ASSET_DIR" "ASSETS" ${ARGN})

    if(NOT EA_OUTPUT_SOURCE)
        message(FATAL_ERROR "embed_assets: OUTPUT_SOURCE required")
    endif()
    if(NOT EA_ASSET_DIR)
        message(FATAL_ERROR "embed_assets: ASSET_DIR required")
    endif()
    if(NOT EA_ASSETS)
        message(FATAL_ERROR "embed_assets: ASSETS required")
    endif()

    set(_source_content "// Auto-generated embedded asset data. Do not edit.\n")
    string(APPEND _source_content "// NOLINTBEGIN\n")
    string(APPEND _source_content "#include \"runtime/embedded_assets.hpp\"\n\n")
    string(APPEND _source_content "#include <array>\n")
    string(APPEND _source_content "#include <string_view>\n\n")
    string(APPEND _source_content "namespace goggles::filter_chain::runtime {\n\n")
    string(APPEND _source_content "namespace {\n\n")

    set(_asset_count 0)
    set(_registry_entries "")

    foreach(_asset_spec IN LISTS EA_ASSETS)
        # Parse "asset_id:relative_path"
        string(FIND "${_asset_spec}" ":" _colon_pos)
        if(_colon_pos EQUAL -1)
            message(FATAL_ERROR "embed_assets: Invalid spec '${_asset_spec}'. Expected 'id:path'.")
        endif()

        string(SUBSTRING "${_asset_spec}" 0 ${_colon_pos} _asset_id)
        math(EXPR _path_start "${_colon_pos} + 1")
        string(SUBSTRING "${_asset_spec}" ${_path_start} -1 _relative_path)

        set(_full_path "${EA_ASSET_DIR}/${_relative_path}")
        if(NOT EXISTS "${_full_path}")
            message(FATAL_ERROR "embed_assets: Asset not found: ${_full_path}")
        endif()

        file(READ "${_full_path}" _file_content)
        string(LENGTH "${_file_content}" _file_size)

        # Convert to hex byte array
        file(READ "${_full_path}" _hex_content HEX)
        string(LENGTH "${_hex_content}" _hex_len)

        # Build C array variable name from asset_id (replace / and . with _)
        string(REPLACE "/" "_" _var_name "${_asset_id}")
        string(REPLACE "." "_" _var_name "${_var_name}")
        string(REPLACE "-" "_" _var_name "${_var_name}")
        set(_var_name "k_asset_${_var_name}")

        string(APPEND _source_content "// Asset: ${_asset_id} (${_file_size} bytes)\n")
        string(APPEND _source_content "constexpr uint8_t ${_var_name}[] = {\n    ")

        set(_col 0)
        set(_pos 0)
        while(_pos LESS _hex_len)
            math(EXPR _end "${_pos} + 2")
            string(SUBSTRING "${_hex_content}" ${_pos} 2 _byte)
            string(APPEND _source_content "0x${_byte},")

            math(EXPR _col "${_col} + 1")
            if(_col EQUAL 16)
                string(APPEND _source_content "\n    ")
                set(_col 0)
            else()
                string(APPEND _source_content " ")
            endif()

            math(EXPR _pos "${_pos} + 2")
        endwhile()

        string(APPEND _source_content "\n};\n\n")

        # Add to registry
        string(APPEND _registry_entries
            "    EmbeddedAsset{\"${_asset_id}\", std::span<const uint8_t>{${_var_name}, sizeof(${_var_name})}},\n")
        math(EXPR _asset_count "${_asset_count} + 1")
    endforeach()

    string(APPEND _source_content "constexpr std::array<EmbeddedAsset, ${_asset_count}> k_registry = {{\n")
    string(APPEND _source_content "${_registry_entries}")
    string(APPEND _source_content "}};\n\n")
    string(APPEND _source_content "} // namespace\n\n")

    string(APPEND _source_content "auto EmbeddedAssetRegistry::find(std::string_view asset_id) -> std::optional<EmbeddedAsset> {\n")
    string(APPEND _source_content "    for (const auto& entry : k_registry) {\n")
    string(APPEND _source_content "        if (entry.asset_id == asset_id) {\n")
    string(APPEND _source_content "            return entry;\n")
    string(APPEND _source_content "        }\n")
    string(APPEND _source_content "    }\n")
    string(APPEND _source_content "    return std::nullopt;\n")
    string(APPEND _source_content "}\n\n")

    string(APPEND _source_content "auto EmbeddedAssetRegistry::count() -> size_t {\n")
    string(APPEND _source_content "    return k_registry.size();\n")
    string(APPEND _source_content "}\n\n")

    string(APPEND _source_content "} // namespace goggles::filter_chain::runtime\n")
    string(APPEND _source_content "// NOLINTEND\n")

    file(WRITE "${EA_OUTPUT_SOURCE}" "${_source_content}")
endfunction()
