#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <goggles/filter_chain/error.hpp>
#include <goggles_filter_chain.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace goggles::filter_chain::runtime {

/// @brief Provenance tracking for resolved sources.
struct SourceProvenance {
    uint32_t kind = GOGGLES_FC_PROVENANCE_FILE;
    std::string source_name;
    std::string source_path;
};

/// @brief Resolved source bytes with provenance metadata.
struct ResolvedSource {
    std::vector<uint8_t> bytes;
    SourceProvenance provenance;
    std::filesystem::path base_path;
};

/// @brief Resolves preset sources from file or memory, with import callback support.
///
/// Supports file-backed and memory-backed preset sources, provenance tracking,
/// import callbacks, explicit base paths, and deterministic rejection of relative
/// external references with no resolver/base path.
class SourceResolver {
public:
    /// @brief Resolve a preset source descriptor into loaded bytes with provenance.
    [[nodiscard]] auto resolve(const goggles_fc_preset_source_t* source)
        -> goggles::Result<ResolvedSource>;

    /// @brief Resolve a relative import path using the resolver's current context.
    /// Used by the preset parser when processing #reference or shader/texture paths.
    [[nodiscard]] auto resolve_relative(const std::filesystem::path& base_path,
                                        std::string_view relative_path,
                                        const goggles_fc_import_callbacks_t* import_callbacks)
        -> goggles::Result<std::vector<uint8_t>>;

    /// @brief Check whether a relative path can be resolved given the current context.
    /// If neither base_path nor import_callbacks are available, returns false.
    [[nodiscard]] static auto
    can_resolve_relative(const std::filesystem::path& base_path,
                         const goggles_fc_import_callbacks_t* import_callbacks) -> bool;

private:
    [[nodiscard]] auto resolve_file(const goggles_fc_preset_source_t* source)
        -> goggles::Result<ResolvedSource>;

    [[nodiscard]] auto resolve_memory(const goggles_fc_preset_source_t* source)
        -> goggles::Result<ResolvedSource>;
};

} // namespace goggles::filter_chain::runtime
