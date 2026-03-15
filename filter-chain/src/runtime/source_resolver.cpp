#include "source_resolver.hpp"

#include "embedded_assets.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace goggles::filter_chain::runtime {

auto SourceResolver::resolve(const goggles_fc_preset_source_t* source)
    -> goggles::Result<ResolvedSource> {
    if (source->kind == GOGGLES_FC_PRESET_SOURCE_FILE) {
        return resolve_file(source);
    }
    if (source->kind == GOGGLES_FC_PRESET_SOURCE_MEMORY) {
        return resolve_memory(source);
    }
    return goggles::make_error<ResolvedSource>(goggles::ErrorCode::invalid_data,
                                               "Unknown preset source kind");
}

auto SourceResolver::resolve_file(const goggles_fc_preset_source_t* source)
    -> goggles::Result<ResolvedSource> {
    std::string path_str(source->path.data != nullptr ? source->path.data : "",
                         source->path.data != nullptr ? source->path.size : 0);

    // Empty path is passthrough: return empty bytes so ChainBuilder creates a
    // single-pass blit chain.
    if (path_str.empty()) {
        ResolvedSource result;
        result.provenance.kind = GOGGLES_FC_PROVENANCE_FILE;
        result.provenance.source_name = "(passthrough)";
        return result;
    }

    std::filesystem::path preset_path(path_str);

    std::ifstream file(preset_path, std::ios::binary);
    if (!file) {
        return goggles::make_error<ResolvedSource>(goggles::ErrorCode::file_not_found,
                                                   "Failed to open preset: " + path_str);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    ResolvedSource result;
    result.bytes.assign(content.begin(), content.end());
    result.provenance.kind = GOGGLES_FC_PROVENANCE_FILE;
    result.provenance.source_path = path_str;
    result.provenance.source_name = preset_path.filename().string();
    result.base_path = preset_path.parent_path();

    return result;
}

auto SourceResolver::resolve_memory(const goggles_fc_preset_source_t* source)
    -> goggles::Result<ResolvedSource> {
    const auto* byte_data = static_cast<const uint8_t*>(source->bytes);

    ResolvedSource result;
    if (byte_data != nullptr && source->byte_count > 0) {
        result.bytes.assign(byte_data, byte_data + source->byte_count);
    }
    result.provenance.kind = GOGGLES_FC_PROVENANCE_MEMORY;

    if (source->source_name.data != nullptr && source->source_name.size > 0) {
        result.provenance.source_name.assign(source->source_name.data, source->source_name.size);
    }

    if (source->base_path.data != nullptr && source->base_path.size > 0) {
        std::string base_str(source->base_path.data, source->base_path.size);
        result.base_path = std::filesystem::path(base_str);
    }

    return result;
}

auto SourceResolver::resolve_relative(const std::filesystem::path& base_path,
                                      std::string_view relative_path,
                                      const goggles_fc_import_callbacks_t* import_callbacks)
    -> goggles::Result<std::vector<uint8_t>> {
    // Try import callbacks first if available
    if (import_callbacks != nullptr && import_callbacks->read_fn != nullptr) {
        goggles_fc_utf8_view_t rel_view{};
        rel_view.data = relative_path.data();
        rel_view.size = relative_path.size();

        void* out_bytes = nullptr;
        size_t out_byte_count = 0;

        auto import_status = import_callbacks->read_fn(rel_view, &out_bytes, &out_byte_count,
                                                       import_callbacks->user_data);
        if (import_status == GOGGLES_FC_STATUS_OK) {
            std::vector<uint8_t> data;
            if (out_bytes != nullptr && out_byte_count > 0) {
                data.assign(static_cast<const uint8_t*>(out_bytes),
                            static_cast<const uint8_t*>(out_bytes) + out_byte_count);
            }

            if (out_bytes != nullptr && import_callbacks->free_fn != nullptr) {
                import_callbacks->free_fn(out_bytes, out_byte_count, import_callbacks->user_data);
            }

            return data;
        }

        // If callback returned an error, free any buffer it may have allocated
        if (out_bytes != nullptr && import_callbacks->free_fn != nullptr) {
            import_callbacks->free_fn(out_bytes, out_byte_count, import_callbacks->user_data);
        }

        // Propagate the error
        return goggles::make_error<std::vector<uint8_t>>(goggles::ErrorCode::file_not_found,
                                                         "Import callback failed for: " +
                                                             std::string(relative_path));
    }

    // Check embedded assets before filesystem fallback. Built-in shaders and
    // runtime assets are compiled into the library and looked up by asset ID
    // (typically a relative path such as "shaders/output.slang"). This removes
    // the runtime dependency on a public shader_dir.
    if (auto asset = EmbeddedAssetRegistry::find(relative_path)) {
        return std::vector<uint8_t>(asset->data.begin(), asset->data.end());
    }

    // Fall back to filesystem resolution via base_path
    if (!base_path.empty()) {
        // Reject absolute paths to prevent imports from escaping the base directory.
        std::filesystem::path rel_fs_path{std::string{relative_path}};
        if (rel_fs_path.is_absolute()) {
            return goggles::make_error<std::vector<uint8_t>>(goggles::ErrorCode::invalid_data,
                                                             "Absolute import path rejected: " +
                                                                 std::string(relative_path));
        }

        auto full_path = base_path / rel_fs_path;
        std::ifstream file(full_path, std::ios::binary);
        if (!file) {
            return goggles::make_error<std::vector<uint8_t>>(goggles::ErrorCode::file_not_found,
                                                             "Failed to open relative import: " +
                                                                 full_path.string());
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        return std::vector<uint8_t>(content.begin(), content.end());
    }

    // Neither callback nor base_path: deterministic rejection
    return goggles::make_error<std::vector<uint8_t>>(
        goggles::ErrorCode::invalid_config, "Cannot resolve relative path '" +
                                                std::string(relative_path) +
                                                "': no import callback or base_path provided");
}

auto SourceResolver::can_resolve_relative(const std::filesystem::path& base_path,
                                          const goggles_fc_import_callbacks_t* import_callbacks)
    -> bool {
    if (import_callbacks != nullptr && import_callbacks->read_fn != nullptr) {
        return true;
    }
    // Embedded assets are always available as a fallback source, so even
    // without import callbacks or a base_path, built-in assets can be resolved.
    // However, for non-embedded relative paths we still need a base_path.
    return !base_path.empty();
}

} // namespace goggles::filter_chain::runtime
