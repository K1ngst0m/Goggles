#include "render/backend/vulkan_backend.hpp"
#include "render/chain/filter_controls.hpp"
#include "ui/imgui_layer.hpp"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

namespace {

auto read_text_file(const std::filesystem::path& path) -> std::optional<std::string> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

auto collect_app_ui_sources() -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> files;
    std::array<std::filesystem::path, 2> directories = {
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/app",
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/ui",
    };

    for (const auto& dir : directories) {
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec) || ec) {
            continue;
        }
        for (auto it = std::filesystem::recursive_directory_iterator(dir, ec);
             it != std::filesystem::recursive_directory_iterator() && !ec; it.increment(ec)) {
            if (!it->is_regular_file(ec) || ec) {
                continue;
            }
            auto ext = it->path().extension();
            if (ext == ".cpp" || ext == ".hpp") {
                files.push_back(it->path());
            }
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

} // namespace

TEST_CASE("Filter chain boundary control contract coverage", "[filter_chain][boundary_contract]") {
    using goggles::render::FilterControlDescriptor;
    using goggles::render::FilterControlId;
    using goggles::render::FilterControlStage;
    using goggles::render::VulkanBackend;

    using ListAllSig = std::vector<FilterControlDescriptor> (VulkanBackend::*)() const;
    using ListStageSig =
        std::vector<FilterControlDescriptor> (VulkanBackend::*)(FilterControlStage) const;
    using SetSig = bool (VulkanBackend::*)(FilterControlId, float);
    using ResetSig = bool (VulkanBackend::*)(FilterControlId);

    static_assert(
        std::is_same_v<decltype(static_cast<ListAllSig>(&VulkanBackend::list_filter_controls)),
                       ListAllSig>);
    static_assert(
        std::is_same_v<decltype(static_cast<ListStageSig>(&VulkanBackend::list_filter_controls)),
                       ListStageSig>);
    static_assert(std::is_same_v<decltype(&VulkanBackend::set_filter_control_value), SetSig>);
    static_assert(std::is_same_v<decltype(&VulkanBackend::reset_filter_control_value), ResetSig>);

    static_assert(std::is_same_v<goggles::ui::ParameterChangeCallback,
                                 std::function<void(FilterControlId, float)>>);
    static_assert(std::is_same_v<goggles::ui::PreChainParameterCallback,
                                 std::function<void(FilterControlId, float)>>);

    REQUIRE(std::string_view{goggles::render::to_string(FilterControlStage::prechain)} ==
            "prechain");
    REQUIRE(std::string_view{goggles::render::to_string(FilterControlStage::effect)} == "effect");

    const auto app_path = std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/app/application.cpp";
    auto app_text = read_text_file(app_path);
    REQUIRE(app_text.has_value());
    REQUIRE(app_text->find("list_filter_controls(") != std::string::npos);
    REQUIRE(app_text->find("set_filter_control_value(") != std::string::npos);
    REQUIRE((app_text->find("reset_filter_control_value(") != std::string::npos ||
             app_text->find("reset_filter_controls(") != std::string::npos));
    REQUIRE(app_text->find("filter_chain(") == std::string::npos);

    const auto chain_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/chain/filter_chain.cpp";
    auto chain_text = read_text_file(chain_path);
    REQUIRE(chain_text.has_value());

    const auto prechain_list_pos =
        chain_text->find("auto prechain_controls = collect_prechain_controls();");
    const auto effect_list_pos =
        chain_text->find("auto effect_controls = collect_effect_controls();", prechain_list_pos);
    const auto merge_pos = chain_text->find("prechain_controls.insert(", effect_list_pos);
    REQUIRE(prechain_list_pos != std::string::npos);
    REQUIRE(effect_list_pos != std::string::npos);
    REQUIRE(merge_pos != std::string::npos);
    REQUIRE(prechain_list_pos < effect_list_pos);
    REQUIRE(effect_list_pos < merge_pos);

    REQUIRE(
        chain_text->find("const float clamped = clamp_filter_control_value(descriptor, value);") !=
        std::string::npos);

    const auto source_files = collect_app_ui_sources();
    const std::array<std::string_view, 3> forbidden_patterns = {
        "render/chain/filter_chain.hpp",
        "render/shader/",
        "filter_chain(",
    };

    for (const auto& source_path : source_files) {
        auto source_text = read_text_file(source_path);
        REQUIRE(source_text.has_value());
        for (const auto& pattern : forbidden_patterns) {
            INFO("File: " << source_path << ", pattern: " << pattern);
            REQUIRE(source_text->find(pattern) == std::string::npos);
        }
    }
}

TEST_CASE("Async swap and resize safety contract coverage", "[filter_chain][async_contract]") {
    const auto backend_cpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/vulkan_backend.cpp";
    auto backend_text = read_text_file(backend_cpp);
    REQUIRE(backend_text.has_value());

    const auto check_swap_pos =
        backend_text->find("void VulkanBackend::check_pending_chain_swap()");
    REQUIRE(check_swap_pos != std::string::npos);

    const auto failure_branch_pos = backend_text->find("if (!result) {", check_swap_pos);
    const auto failure_reset_pos =
        backend_text->find("m_pending_filter_chain.reset();", failure_branch_pos);
    const auto failure_clear_ready_pos =
        backend_text->find("m_pending_chain_ready.store(false", failure_reset_pos);
    const auto failure_return_pos = backend_text->find("return;", failure_clear_ready_pos);
    const auto success_signal_pos = backend_text->find(
        "m_chain_swapped.store(true, std::memory_order_release);", failure_return_pos);

    REQUIRE(failure_branch_pos != std::string::npos);
    REQUIRE(failure_reset_pos != std::string::npos);
    REQUIRE(failure_clear_ready_pos != std::string::npos);
    REQUIRE(failure_return_pos != std::string::npos);
    REQUIRE(success_signal_pos != std::string::npos);
    REQUIRE(failure_branch_pos < failure_reset_pos);
    REQUIRE(failure_reset_pos < failure_clear_ready_pos);
    REQUIRE(failure_clear_ready_pos < failure_return_pos);
    REQUIRE(failure_return_pos < success_signal_pos);

    const auto swap_reapply_resolution_pos = backend_text->find(
        "m_filter_chain->set_prechain_resolution(m_source_resolution);", check_swap_pos);
    REQUIRE(swap_reapply_resolution_pos != std::string::npos);
    REQUIRE(swap_reapply_resolution_pos < success_signal_pos);

    REQUIRE(backend_text->find(".destroy_after_frame = m_frame_count + MAX_FRAMES_IN_FLIGHT + 1") !=
            std::string::npos);
    REQUIRE(backend_text->find("m_filter_chain->handle_resize(m_swapchain_extent)") !=
            std::string::npos);

    const auto shutdown_pos = backend_text->find("void VulkanBackend::shutdown()");
    const auto pending_shutdown_pos =
        backend_text->find("shutdown_chain(m_pending_filter_chain);", shutdown_pos);
    const auto deferred_shutdown_pos =
        backend_text->find("shutdown_chain(m_deferred_destroys[i].filter_chain);", shutdown_pos);
    const auto device_destroy_pos = backend_text->find("m_device.destroy();", shutdown_pos);

    REQUIRE(shutdown_pos != std::string::npos);
    REQUIRE(pending_shutdown_pos != std::string::npos);
    REQUIRE(deferred_shutdown_pos != std::string::npos);
    REQUIRE(device_destroy_pos != std::string::npos);
    REQUIRE(pending_shutdown_pos < device_destroy_pos);
    REQUIRE(deferred_shutdown_pos < device_destroy_pos);

    const auto backend_hpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/vulkan_backend.hpp";
    auto header_text = read_text_file(backend_hpp);
    REQUIRE(header_text.has_value());
    REQUIRE(header_text->find("m_chain_swapped.exchange(false, std::memory_order_acq_rel)") !=
            std::string::npos);
}
