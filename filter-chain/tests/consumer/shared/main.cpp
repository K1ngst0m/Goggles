// Installed C++ consumer test — shared linkage.
// Validates that the installed GogglesFilterChain C++ wrapper headers compile
// correctly, expose the expected types/namespaces, and satisfy basic type traits.
// No Vulkan device is created — this is a build-level validation, not a runtime test.

#include <cstdint>
#include <goggles_filter_chain.hpp>
#include <string_view>
#include <type_traits>

int main() {
    // ── 1. common.hpp enum and struct validation ────────────────────────────

    // Enum value contracts.
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

    static_assert(static_cast<uint32_t>(goggles::filter_chain::Provenance::file) == 0u,
                  "Provenance::file is 0");
    static_assert(static_cast<uint32_t>(goggles::filter_chain::Provenance::memory) == 1u,
                  "Provenance::memory is 1");

    // Extent2D is a simple POD struct.
    static_assert(std::is_standard_layout_v<goggles::filter_chain::Extent2D>,
                  "Extent2D is standard layout");
    static_assert(std::is_trivially_copyable_v<goggles::filter_chain::Extent2D>,
                  "Extent2D is trivially copyable");

    const goggles::filter_chain::Extent2D ext{.width = 320, .height = 240};
    if (ext.width != 320 || ext.height != 240) {
        return 1;
    }

    // ── 2. Wrapper types are move-only ──────────────────────────────────────

    static_assert(!std::is_copy_constructible_v<goggles::filter_chain::Instance>,
                  "Instance is not copy-constructible");
    static_assert(!std::is_copy_assignable_v<goggles::filter_chain::Instance>,
                  "Instance is not copy-assignable");
    static_assert(std::is_nothrow_move_constructible_v<goggles::filter_chain::Instance>,
                  "Instance is nothrow move-constructible");
    static_assert(std::is_nothrow_move_assignable_v<goggles::filter_chain::Instance>,
                  "Instance is nothrow move-assignable");

    static_assert(!std::is_copy_constructible_v<goggles::filter_chain::Device>,
                  "Device is not copy-constructible");
    static_assert(std::is_nothrow_move_constructible_v<goggles::filter_chain::Device>,
                  "Device is nothrow move-constructible");

    static_assert(!std::is_copy_constructible_v<goggles::filter_chain::Program>,
                  "Program is not copy-constructible");
    static_assert(std::is_nothrow_move_constructible_v<goggles::filter_chain::Program>,
                  "Program is nothrow move-constructible");

    static_assert(!std::is_copy_constructible_v<goggles::filter_chain::Chain>,
                  "Chain is not copy-constructible");
    static_assert(std::is_nothrow_move_constructible_v<goggles::filter_chain::Chain>,
                  "Chain is nothrow move-constructible");

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

    // ── 3. Version and capability queries ───────────────────────────────────

    const uint32_t api_ver = goggles::filter_chain::get_api_version();
    const uint32_t abi_ver = goggles::filter_chain::get_abi_version();
    const auto caps = goggles::filter_chain::get_capabilities();

    if (api_ver == 0 || abi_ver == 0) {
        return 1;
    }

    // Vulkan capability is always present.
    if ((caps & GOGGLES_FC_CAPABILITY_VULKAN) == 0) {
        return 1;
    }

    // ── 4. status_string works ──────────────────────────────────────────────

    const char* ok_str = goggles::filter_chain::status_string(GOGGLES_FC_STATUS_OK);
    const char* err_str = goggles::filter_chain::status_string(GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    if (ok_str == nullptr || ok_str[0] == '\0') {
        return 1;
    }
    if (err_str == nullptr || err_str[0] == '\0') {
        return 1;
    }

    // ── 5. Default-constructed wrapper is null ──────────────────────────────

    {
        goggles::filter_chain::Instance inst;
        if (static_cast<bool>(inst) || inst.handle() != nullptr) {
            return 1; // default must be null
        }

        // Move construction from default is safe (moves null).
        goggles::filter_chain::Instance moved = std::move(inst);
        if (static_cast<bool>(moved)) {
            return 1;
        }
    }

    return 0;
}
