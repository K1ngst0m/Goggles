// Installed C++ consumer test — static linkage.
// Validates that the installed GogglesFilterChain C++ wrapper headers compile
// correctly, expose the expected types/namespaces, and satisfy basic type traits.
// No Vulkan device is created — this is a build-level validation, not a runtime test.

#include <cstdint>
#include <goggles_filter_chain.hpp>
#include <string_view>
#include <type_traits>

int main() {
    // ── 1. Namespace and type existence ─────────────────────────────────────

    // RAII wrapper types exist in the correct namespace and hold a pointer-sized handle.
    static_assert(sizeof(goggles::filter_chain::Instance) >= sizeof(void*),
                  "Instance holds at least a pointer");
    static_assert(sizeof(goggles::filter_chain::Device) >= sizeof(void*),
                  "Device holds at least a pointer");
    static_assert(sizeof(goggles::filter_chain::Program) >= sizeof(void*),
                  "Program holds at least a pointer");
    static_assert(sizeof(goggles::filter_chain::Chain) >= sizeof(void*),
                  "Chain holds at least a pointer");

    // ── 2. Move-only semantics (not copyable) ───────────────────────────────

    static_assert(!std::is_copy_constructible_v<goggles::filter_chain::Instance>,
                  "Instance is not copy-constructible");
    static_assert(!std::is_copy_assignable_v<goggles::filter_chain::Instance>,
                  "Instance is not copy-assignable");
    static_assert(std::is_move_constructible_v<goggles::filter_chain::Instance>,
                  "Instance is move-constructible");
    static_assert(std::is_move_assignable_v<goggles::filter_chain::Instance>,
                  "Instance is move-assignable");

    static_assert(!std::is_copy_constructible_v<goggles::filter_chain::Device>,
                  "Device is not copy-constructible");
    static_assert(!std::is_copy_assignable_v<goggles::filter_chain::Device>,
                  "Device is not copy-assignable");
    static_assert(std::is_move_constructible_v<goggles::filter_chain::Device>,
                  "Device is move-constructible");
    static_assert(std::is_move_assignable_v<goggles::filter_chain::Device>,
                  "Device is move-assignable");

    static_assert(!std::is_copy_constructible_v<goggles::filter_chain::Program>,
                  "Program is not copy-constructible");
    static_assert(!std::is_copy_assignable_v<goggles::filter_chain::Program>,
                  "Program is not copy-assignable");
    static_assert(std::is_move_constructible_v<goggles::filter_chain::Program>,
                  "Program is move-constructible");
    static_assert(std::is_move_assignable_v<goggles::filter_chain::Program>,
                  "Program is move-assignable");

    static_assert(!std::is_copy_constructible_v<goggles::filter_chain::Chain>,
                  "Chain is not copy-constructible");
    static_assert(!std::is_copy_assignable_v<goggles::filter_chain::Chain>,
                  "Chain is not copy-assignable");
    static_assert(std::is_move_constructible_v<goggles::filter_chain::Chain>,
                  "Chain is move-constructible");
    static_assert(std::is_move_assignable_v<goggles::filter_chain::Chain>,
                  "Chain is move-assignable");

    using FindControlIndexSignature = goggles::Result<uint32_t> (goggles::filter_chain::Chain::*)(
        goggles::filter_chain::Stage, std::string_view) const;
    using SetControlByNameSignature = goggles::Result<void> (goggles::filter_chain::Chain::*)(
        goggles::filter_chain::Stage, std::string_view, float);

    FindControlIndexSignature find_control_index_member =
        &goggles::filter_chain::Chain::find_control_index;
    SetControlByNameSignature set_control_by_name_member = static_cast<SetControlByNameSignature>(
        &goggles::filter_chain::Chain::set_control_value_f32);
    (void)find_control_index_member;
    (void)set_control_by_name_member;

    // ── 3. Alignment checks ────────────────────────────────────────────────

    static_assert(alignof(goggles::filter_chain::Instance) >= alignof(void*),
                  "Instance is at least pointer-aligned");
    static_assert(alignof(goggles::filter_chain::Device) >= alignof(void*),
                  "Device is at least pointer-aligned");
    static_assert(alignof(goggles::filter_chain::Program) >= alignof(void*),
                  "Program is at least pointer-aligned");
    static_assert(alignof(goggles::filter_chain::Chain) >= alignof(void*),
                  "Chain is at least pointer-aligned");

    // ── 4. common.hpp enums exist in the correct namespace ──────────────────

    static_assert(static_cast<uint32_t>(goggles::filter_chain::LogLevel::trace) == 0u,
                  "LogLevel::trace is 0");
    static_assert(static_cast<uint32_t>(goggles::filter_chain::LogLevel::critical) == 5u,
                  "LogLevel::critical is 5");

    static_assert(static_cast<uint32_t>(goggles::filter_chain::Stage::prechain) == 0u,
                  "Stage::prechain is 0");
    static_assert(static_cast<uint32_t>(goggles::filter_chain::Stage::effect) == 1u,
                  "Stage::effect is 1");
    static_assert(static_cast<uint32_t>(goggles::filter_chain::Stage::postchain) == 2u,
                  "Stage::postchain is 2");

    static_assert(static_cast<uint32_t>(goggles::filter_chain::ScaleMode::stretch) == 0u,
                  "ScaleMode::stretch is 0");
    static_assert(static_cast<uint32_t>(goggles::filter_chain::ScaleMode::fit) == 1u,
                  "ScaleMode::fit is 1");
    static_assert(static_cast<uint32_t>(goggles::filter_chain::ScaleMode::integer) == 2u,
                  "ScaleMode::integer is 2");
    static_assert(static_cast<uint32_t>(goggles::filter_chain::ScaleMode::fill) == 3u,
                  "ScaleMode::fill is 3");
    static_assert(static_cast<uint32_t>(goggles::filter_chain::ScaleMode::dynamic) == 4u,
                  "ScaleMode::dynamic is 4");

    static_assert(static_cast<uint32_t>(goggles::filter_chain::PresetSourceKind::file) == 0u,
                  "PresetSourceKind::file is 0");
    static_assert(static_cast<uint32_t>(goggles::filter_chain::PresetSourceKind::memory) == 1u,
                  "PresetSourceKind::memory is 1");

    // ── 5. Extent2D struct ──────────────────────────────────────────────────

    goggles::filter_chain::Extent2D extent{.width = 1920, .height = 1080};
    if (extent.width != 1920 || extent.height != 1080) {
        return 1;
    }

    // ── 6. Version queries (runtime check via C++ inline wrappers) ──────────

    const uint32_t api_version = goggles::filter_chain::get_api_version();
    if (api_version == 0) {
        return 1;
    }

    const uint32_t abi_version = goggles::filter_chain::get_abi_version();
    if (abi_version == 0) {
        return 1;
    }

    const auto capabilities = goggles::filter_chain::get_capabilities();
    // Vulkan capability must always be present.
    if ((capabilities & GOGGLES_FC_CAPABILITY_VULKAN) == 0) {
        return 1;
    }

    // ── 7. status_string (runtime check) ────────────────────────────────────

    const char* ok_str = goggles::filter_chain::status_string(GOGGLES_FC_STATUS_OK);
    if (ok_str == nullptr || ok_str[0] == '\0') {
        return 1;
    }

    // ── 8. Default-constructed wrappers are null/false ───────────────────────

    goggles::filter_chain::Instance default_instance;
    if (static_cast<bool>(default_instance)) {
        return 1; // default instance should be null
    }
    if (default_instance.handle() != nullptr) {
        return 1;
    }

    return 0;
}
