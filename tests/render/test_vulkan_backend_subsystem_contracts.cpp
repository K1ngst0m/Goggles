#include "render/backend/external_frame_importer.hpp"
#include "render/backend/filter_chain_controller.hpp"
#include "render/backend/render_output.hpp"
#include "render/backend/vulkan_context.hpp"
#include "render/chain/vulkan_context.hpp"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
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

auto collect_render_boundary_sources() -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> files;
    std::array<std::filesystem::path, 3> directories = {
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/chain",
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/shader",
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/texture",
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

            const auto ext = it->path().extension();
            if (ext == ".cpp" || ext == ".hpp") {
                files.push_back(it->path());
            }
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

auto find_text(std::string_view text, std::string_view needle, size_t offset = 0) -> size_t {
    return text.find(needle, offset);
}

} // namespace

TEST_CASE("Vulkan backend seam declarations stay compile-safe", "[vulkan-backend-module-layout]") {
    namespace backend_internal = goggles::render::backend_internal;

    static_assert(!std::is_same_v<backend_internal::VulkanContext, goggles::render::VulkanContext>);
    static_assert(std::is_same_v<backend_internal::FilterChainController::BoundaryVulkanContext,
                                 goggles::render::VulkanContext>);
    static_assert(backend_internal::RenderOutput::MAX_FRAMES_IN_FLIGHT == 2u);
    using BoundaryContextAdapterSig =
        goggles::render::VulkanContext (backend_internal::VulkanContext::*)(vk::CommandPool) const;
    static_assert(std::is_same_v<decltype(&backend_internal::VulkanContext::boundary_context),
                                 BoundaryContextAdapterSig>);

    backend_internal::VulkanContext context{};
    backend_internal::RenderOutput output{};
    backend_internal::ExternalFrameImporter importer{};
    backend_internal::FilterChainController controller{};
    const auto boundary_context = context.boundary_context(vk::CommandPool{});

    REQUIRE(context.graphics_queue_family == UINT32_MAX);
    REQUIRE(output.command_pool == vk::CommandPool{});
    REQUIRE(output.current_frame == 0u);
    REQUIRE(output.target_fps == 0u);
    REQUIRE(output.swapchain_format == vk::Format::eUndefined);
    REQUIRE(importer.import_extent == vk::Extent2D{});
    REQUIRE(importer.source_format == vk::Format::eUndefined);
    REQUIRE(importer.wait_semaphore(0) == vk::Semaphore{});
    REQUIRE(controller.prechain_policy_enabled);
    REQUIRE(controller.effect_stage_policy_enabled);
    REQUIRE(controller.deferred_count == 0u);
    REQUIRE(controller.pending_preset_path.empty());
    REQUIRE(boundary_context.command_pool == vk::CommandPool{});
    REQUIRE(boundary_context.device == vk::Device{});
}

TEST_CASE("Vulkan backend dependency edge audits stay explicit", "[vulkan-backend-module-layout]") {
    const auto backend_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend";

    const auto render_output_text = read_text_file(backend_root / "render_output.hpp");
    const auto importer_text = read_text_file(backend_root / "external_frame_importer.hpp");
    const auto importer_cpp_text = read_text_file(backend_root / "external_frame_importer.cpp");
    const auto controller_text = read_text_file(backend_root / "filter_chain_controller.hpp");
    const auto backend_header_text = read_text_file(backend_root / "vulkan_backend.hpp");

    REQUIRE(render_output_text.has_value());
    REQUIRE(importer_text.has_value());
    REQUIRE(importer_cpp_text.has_value());
    REQUIRE(controller_text.has_value());
    REQUIRE(backend_header_text.has_value());

    REQUIRE(find_text(*render_output_text, "#include \"vulkan_context.hpp\"") != std::string::npos);
    REQUIRE(find_text(*render_output_text, "external_frame_importer.hpp") == std::string::npos);
    REQUIRE(find_text(*render_output_text, "filter_chain_controller.hpp") == std::string::npos);
    REQUIRE(find_text(*render_output_text, "pending_acquire_sync_sem") == std::string::npos);
    REQUIRE(find_text(*render_output_text, "target_extent()") != std::string::npos);
    REQUIRE(find_text(*render_output_text, "clear_resize_request()") != std::string::npos);
    REQUIRE(find_text(*importer_text, "#include \"vulkan_context.hpp\"") != std::string::npos);
    REQUIRE(find_text(*importer_text, "render_output.hpp") == std::string::npos);
    REQUIRE(find_text(*importer_text, "filter_chain_controller.hpp") == std::string::npos);
    REQUIRE(find_text(*importer_text, "pending_wait_semaphores") != std::string::npos);
    REQUIRE(find_text(*importer_text, "clear_current_source()") != std::string::npos);
    REQUIRE(find_text(*importer_cpp_text, "prepare_wait_semaphore") != std::string::npos);
    REQUIRE(find_text(*importer_cpp_text, "retire_wait_semaphore") != std::string::npos);
    REQUIRE(find_text(*importer_cpp_text, "clear_current_source()") != std::string::npos);
    REQUIRE(find_text(*controller_text, "render/chain/vulkan_context.hpp") != std::string::npos);
    REQUIRE(find_text(*controller_text, "render_output.hpp") == std::string::npos);
    REQUIRE(find_text(*controller_text, "external_frame_importer.hpp") == std::string::npos);
    REQUIRE(find_text(*backend_header_text, "m_gpu_selector") == std::string::npos);
    REQUIRE(find_text(*backend_header_text, "MAX_FRAMES_IN_FLIGHT =") == std::string::npos);

    const auto boundary_sources = collect_render_boundary_sources();
    for (const auto& source_path : boundary_sources) {
        auto source_text = read_text_file(source_path);
        REQUIRE(source_text.has_value());
        INFO("File: " << source_path);
        REQUIRE(find_text(*source_text, "render/backend/vulkan_context.hpp") == std::string::npos);
    }
}

TEST_CASE("Vulkan backend teardown audit hooks stay aligned with shutdown order",
          "[vulkan-backend-lifetime]") {
    const auto backend_cpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/vulkan_backend.cpp";
    auto backend_text = read_text_file(backend_cpp);
    REQUIRE(backend_text.has_value());

    const auto context_cpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/vulkan_context.cpp";
    const auto controller_cpp = std::filesystem::path(GOGGLES_SOURCE_DIR) /
                                "src/render/backend/filter_chain_controller.cpp";
    const auto render_output_cpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/render_output.cpp";
    auto context_text = read_text_file(context_cpp);
    auto controller_text = read_text_file(controller_cpp);
    auto render_output_text = read_text_file(render_output_cpp);
    REQUIRE(context_text.has_value());
    REQUIRE(controller_text.has_value());
    REQUIRE(render_output_text.has_value());

    const auto shutdown_pos = find_text(*backend_text, "void VulkanBackend::shutdown()");
    const auto controller_shutdown_pos =
        find_text(*backend_text, "m_filter_chain_controller.shutdown(", shutdown_pos);
    const auto importer_cleanup_pos =
        find_text(*backend_text, "m_external_frame_importer.destroy(m_vulkan_context);",
                  controller_shutdown_pos);
    const auto output_cleanup_pos = find_text(
        *backend_text, "m_render_output.destroy(m_vulkan_context);", importer_cleanup_pos);
    const auto context_destroy_pos =
        find_text(*backend_text, "m_vulkan_context.destroy();", output_cleanup_pos);

    const auto controller_shutdown_impl_pos =
        find_text(*controller_text, "void FilterChainController::shutdown(");
    const auto clear_ready_pos = find_text(*controller_text, "pending_chain_ready.store(false",
                                           controller_shutdown_impl_pos);
    const auto wait_idle_pos = find_text(*controller_text, "wait_for_gpu_idle();", clear_ready_pos);
    const auto active_chain_pos =
        find_text(*controller_text, "destroy_filter_chain(filter_chain", wait_idle_pos);
    const auto pending_chain_pos =
        find_text(*controller_text, "destroy_filter_chain(pending_filter_chain", active_chain_pos);
    const auto deferred_chain_pos =
        find_text(*controller_text, "destroy_filter_chain(deferred_destroys[i].filter_chain",
                  pending_chain_pos);

    const auto output_shutdown_pos =
        find_text(*render_output_text, "void RenderOutput::destroy(VulkanContext& context)");
    const auto offscreen_destroy_pos = find_text(
        *render_output_text, "destroy_offscreen_target(device, *this);", output_shutdown_pos);
    const auto frame_destroy_pos = find_text(
        *render_output_text, "device.destroyFence(frame.in_flight_fence);", offscreen_destroy_pos);
    const auto swapchain_destroy_pos =
        find_text(*render_output_text, "cleanup_swapchain(context);", frame_destroy_pos);
    const auto command_pool_destroy_pos = find_text(
        *render_output_text, "device.destroyCommandPool(command_pool);", swapchain_destroy_pos);

    const auto context_shutdown_pos = find_text(*context_text, "void VulkanContext::destroy()");
    const auto device_destroy_pos =
        find_text(*context_text, "device.destroy();", context_shutdown_pos);
    const auto surface_destroy_pos =
        find_text(*context_text, "instance.destroySurfaceKHR(surface);", device_destroy_pos);
    const auto debug_destroy_pos =
        find_text(*context_text, "debug_messenger.reset();", surface_destroy_pos);
    const auto instance_destroy_pos =
        find_text(*context_text, "instance.destroy();", debug_destroy_pos);

    REQUIRE(shutdown_pos != std::string::npos);
    REQUIRE(controller_shutdown_pos != std::string::npos);
    REQUIRE(controller_shutdown_impl_pos != std::string::npos);
    REQUIRE(clear_ready_pos != std::string::npos);
    REQUIRE(wait_idle_pos != std::string::npos);
    REQUIRE(active_chain_pos != std::string::npos);
    REQUIRE(pending_chain_pos != std::string::npos);
    REQUIRE(deferred_chain_pos != std::string::npos);
    REQUIRE(importer_cleanup_pos != std::string::npos);
    REQUIRE(output_cleanup_pos != std::string::npos);
    REQUIRE(context_destroy_pos != std::string::npos);
    REQUIRE(output_shutdown_pos != std::string::npos);
    REQUIRE(offscreen_destroy_pos != std::string::npos);
    REQUIRE(frame_destroy_pos != std::string::npos);
    REQUIRE(swapchain_destroy_pos != std::string::npos);
    REQUIRE(command_pool_destroy_pos != std::string::npos);
    REQUIRE(context_shutdown_pos != std::string::npos);
    REQUIRE(device_destroy_pos != std::string::npos);
    REQUIRE(surface_destroy_pos != std::string::npos);
    REQUIRE(debug_destroy_pos != std::string::npos);
    REQUIRE(instance_destroy_pos != std::string::npos);

    REQUIRE(controller_shutdown_pos < importer_cleanup_pos);
    REQUIRE(clear_ready_pos < wait_idle_pos);
    REQUIRE(wait_idle_pos < active_chain_pos);
    REQUIRE(active_chain_pos < pending_chain_pos);
    REQUIRE(pending_chain_pos < deferred_chain_pos);
    REQUIRE(deferred_chain_pos < importer_cleanup_pos);
    REQUIRE(importer_cleanup_pos < output_cleanup_pos);
    REQUIRE(output_cleanup_pos < context_destroy_pos);
    REQUIRE(offscreen_destroy_pos < frame_destroy_pos);
    REQUIRE(frame_destroy_pos < swapchain_destroy_pos);
    REQUIRE(swapchain_destroy_pos < command_pool_destroy_pos);
    REQUIRE(device_destroy_pos < surface_destroy_pos);
    REQUIRE(surface_destroy_pos < debug_destroy_pos);
    REQUIRE(debug_destroy_pos < instance_destroy_pos);
}
