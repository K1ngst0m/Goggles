#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace goggles::filter_chain::runtime {

/// @brief A compiled-in asset entry in the embedded asset registry.
struct EmbeddedAsset {
    std::string_view asset_id;
    std::span<const uint8_t> data;
};

/// @brief Registry of built-in shaders and runtime assets compiled into the library.
///
/// Assets are registered at library load time and looked up by internal asset IDs
/// rather than repository-relative paths. This removes the runtime dependency on
/// a public shader_dir input.
class EmbeddedAssetRegistry {
public:
    /// @brief Look up an embedded asset by its internal asset ID.
    /// @return The asset data if found, or std::nullopt.
    [[nodiscard]] static auto find(std::string_view asset_id) -> std::optional<EmbeddedAsset>;

    /// @brief Return the number of registered embedded assets.
    [[nodiscard]] static auto count() -> size_t;
};

} // namespace goggles::filter_chain::runtime
