#pragma once

#include "source_resolver.hpp"

#include <cstdint>
#include <goggles_filter_chain.h>
#include <string>

namespace goggles::filter_chain::runtime {

class Device;

/// @brief Internal program implementation backing `goggles_fc_program_t`.
///
/// Owns immutable parsed preset state, source provenance, and cached count
/// metadata derived from CPU-only preset parsing. A Program is affine to
/// exactly one Device and carries that affinity for its full lifetime.
///
/// Program is a lightweight CPU-only artifact — it does NOT own any GPU
/// resources (pipelines, descriptors, buffers, textures). Chains build their
/// own GPU state independently from `program->resolved_source()`.
///
/// A single Program handle MAY be shared across multiple chains created from
/// the same device. It MUST NOT be attached to chains on any other device.
class Program {
public:
    [[nodiscard]] static auto create(Device* device, const goggles_fc_preset_source_t* source,
                                     goggles_fc_program_t** out_program) -> goggles_fc_status_t;

    ~Program();

    Program(const Program&) = delete;
    auto operator=(const Program&) -> Program& = delete;
    Program(Program&&) = delete;
    auto operator=(Program&&) -> Program& = delete;

    /// @brief Return the owning device.
    [[nodiscard]] auto device() const -> Device* { return m_device; }

    /// @brief Populate a program source info struct for the caller.
    [[nodiscard]] auto get_source_info(goggles_fc_program_source_info_t* out) const
        -> goggles_fc_status_t;

    /// @brief Populate a program report struct for the caller.
    [[nodiscard]] auto get_report(goggles_fc_program_report_t* out) const -> goggles_fc_status_t;

    /// @brief Return the source provenance.
    [[nodiscard]] auto provenance() const -> const SourceProvenance& { return m_provenance; }

    /// @brief Return the resolved source bytes and base path used to compile the program.
    [[nodiscard]] auto resolved_source() const -> const ResolvedSource& {
        return m_resolved_source;
    }

    /// @brief Return the optional import callbacks captured from program creation.
    [[nodiscard]] auto import_callbacks() const -> const goggles_fc_import_callbacks_t* {
        return m_import_callbacks.has_value() ? &*m_import_callbacks : nullptr;
    }

    /// @brief Return the source name for diagnostics.
    [[nodiscard]] auto source_name() const -> const std::string& {
        return m_provenance.source_name;
    }

    /// @brief Return the source path (file-backed only).
    [[nodiscard]] auto source_path() const -> const std::string& {
        return m_provenance.source_path;
    }

    /// @brief Return the number of passes parsed from the preset.
    [[nodiscard]] auto pass_count() const -> uint32_t { return m_pass_count; }

    /// @brief Return the number of shaders (vertex + fragment per pass).
    [[nodiscard]] auto shader_count() const -> uint32_t { return m_shader_count; }

    /// @brief Return the number of textures referenced by the preset.
    [[nodiscard]] auto texture_count() const -> uint32_t { return m_texture_count; }

    /// @brief Return the raw pointer suitable for casting to goggles_fc_program_t*.
    [[nodiscard]] auto as_handle() -> goggles_fc_program_t*;

    /// @brief Recover the Program from an opaque handle.
    [[nodiscard]] static auto from_handle(goggles_fc_program_t* handle) -> Program*;
    [[nodiscard]] static auto from_handle(const goggles_fc_program_t* handle) -> const Program*;

    /// @brief Check whether the opaque handle points to a live Program (magic validation).
    [[nodiscard]] static auto check_magic(const void* handle) -> bool {
        if (handle == nullptr) {
            return false;
        }
        return static_cast<const Program*>(handle)->m_magic == PROGRAM_MAGIC;
    }

private:
    Program() = default;

    Device* m_device = nullptr;
    SourceProvenance m_provenance;
    ResolvedSource m_resolved_source;
    std::optional<goggles_fc_import_callbacks_t> m_import_callbacks;

    uint32_t m_pass_count = 0;
    uint32_t m_shader_count = 0;
    uint32_t m_texture_count = 0;

    static constexpr uint32_t PROGRAM_MAGIC = 0x47464350u; // "GFCP"
    uint32_t m_magic = PROGRAM_MAGIC;
};

} // namespace goggles::filter_chain::runtime
