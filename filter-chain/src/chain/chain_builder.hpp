#pragma once

#include "diagnostics/compile_report.hpp"
#include "diagnostics/diagnostic_session.hpp"
#include "filter_pass.hpp"
#include "frame_history.hpp"
#include "preset_parser.hpp"
#include "texture/texture_loader.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace goggles::filter_chain::runtime {
struct ResolvedSource;
class SourceResolver;
} // namespace goggles::filter_chain::runtime

namespace goggles::render {

class ShaderRuntime;

/// @brief Texture plus sampler bound into a filter chain.
struct LoadedTexture {
    TextureData data;
    vk::Sampler sampler;
};

/// @brief Result of compiling a preset into passes and resources.
struct CompiledChain {
    PresetConfig preset;
    std::vector<std::unique_ptr<FilterPass>> passes;
    std::vector<diagnostics::CompileReport> compile_reports;
    std::unordered_map<std::string, size_t> alias_to_pass_index;
    uint32_t required_history_depth = 0;
    std::unordered_map<std::string, LoadedTexture> texture_registry;
    std::unordered_set<size_t> feedback_pass_indices;
};

/// @brief Compiles a preset file into passes, textures, and metadata.
class ChainBuilder {
public:
    /// @brief Build from a filesystem path (legacy path for ChainRuntime).
    [[nodiscard]] static auto build(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                                    uint32_t num_sync_indices, TextureLoader& texture_loader,
                                    const std::filesystem::path& preset_path,
                                    diagnostics::DiagnosticSession* session = nullptr)
        -> Result<CompiledChain>;

    /// @brief Build from a pre-resolved source (new path for Program/SourceResolver).
    ///
    /// Uses the SourceResolver for relative #reference and import resolution
    /// instead of direct filesystem access. This is the integration point for
    /// file-backed and memory-backed preset sources through the new runtime API.
    [[nodiscard]] static auto build(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                                    uint32_t num_sync_indices, TextureLoader& texture_loader,
                                    const filter_chain::runtime::ResolvedSource& resolved,
                                    filter_chain::runtime::SourceResolver& resolver,
                                    const goggles_fc_import_callbacks_t* import_callbacks,
                                    diagnostics::DiagnosticSession* session = nullptr)
        -> Result<CompiledChain>;

private:
    [[nodiscard]] static auto load_preset_textures(const VulkanContext& vk_ctx,
                                                   TextureLoader& texture_loader,
                                                   const PresetConfig& preset)
        -> Result<std::unordered_map<std::string, LoadedTexture>>;
    [[nodiscard]] static auto
    load_preset_textures(const VulkanContext& vk_ctx, TextureLoader& texture_loader,
                         const PresetConfig& preset,
                         filter_chain::runtime::SourceResolver& resolver,
                         const goggles_fc_import_callbacks_t* import_callbacks)
        -> Result<std::unordered_map<std::string, LoadedTexture>>;
    [[nodiscard]] static auto create_texture_sampler(const VulkanContext& vk_ctx,
                                                     const TextureConfig& config)
        -> Result<vk::Sampler>;
};

} // namespace goggles::render
