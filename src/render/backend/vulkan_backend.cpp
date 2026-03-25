#include "vulkan_backend.hpp"

#include "external_frame_importer.hpp"
#include "filter_chain_controller.hpp"
#include "render_output.hpp"
#include "vulkan_context.hpp"
#include "vulkan_error.hpp"

#include <algorithm>
#include <array>
#include <goggles/profiling.hpp>
#include <util/logging.hpp>

namespace goggles::render {

namespace {

auto to_fc_scale_mode(ScaleMode scale_mode) -> uint32_t {
    switch (scale_mode) {
    case ScaleMode::fit:
        return GOGGLES_FC_SCALE_MODE_FIT;
    case ScaleMode::fill:
        return GOGGLES_FC_SCALE_MODE_FILL;
    case ScaleMode::integer:
        return GOGGLES_FC_SCALE_MODE_INTEGER;
    case ScaleMode::dynamic:
        return GOGGLES_FC_SCALE_MODE_DYNAMIC;
    case ScaleMode::stretch:
    default:
        return GOGGLES_FC_SCALE_MODE_STRETCH;
    }
}

auto resolve_record_integer_scale(ScaleMode scale_mode, uint32_t integer_scale,
                                  vk::Extent2D source_extent, vk::Extent2D target_extent)
    -> uint32_t {
    if (scale_mode != ScaleMode::integer || integer_scale > 0u) {
        return integer_scale;
    }

    if (source_extent.width == 0u || source_extent.height == 0u) {
        return 1u;
    }

    const uint32_t max_scale_x = target_extent.width / source_extent.width;
    const uint32_t max_scale_y = target_extent.height / source_extent.height;
    return std::max(1u, std::min(max_scale_x, max_scale_y));
}

} // namespace

VulkanBackend::~VulkanBackend() {
    shutdown();
}

void VulkanBackend::initialize_paths(const std::filesystem::path& cache_dir) {
    m_cache_dir = cache_dir;
    if (!m_cache_dir.empty()) {
        return;
    }

    try {
        m_cache_dir = std::filesystem::temp_directory_path() / "goggles" / "shaders";
    } catch (...) {
        m_cache_dir = "/tmp/goggles/shaders";
    }
}

void VulkanBackend::initialize_settings(const RenderSettings& settings) {
    m_scale_mode = settings.scale_mode;
    m_integer_scale = settings.integer_scale;
    update_target_fps(settings.target_fps);
    m_filter_chain_controller.set_prechain_resolution(
        backend_internal::FilterChainController::PrechainResolutionConfig{
            .requested_resolution = vk::Extent2D{settings.source_width, settings.source_height},
        });
}

auto VulkanBackend::create(SDL_Window* window, bool enable_validation,
                           const std::filesystem::path& cache_dir, const RenderSettings& settings)
    -> ResultPtr<VulkanBackend> {
    GOGGLES_PROFILE_FUNCTION();

    auto backend = std::unique_ptr<VulkanBackend>(new VulkanBackend());

    backend->initialize_paths(cache_dir);

    auto context_result =
        backend_internal::VulkanContext::create(window, enable_validation, settings.gpu_selector);
    if (!context_result) {
        return nonstd::make_unexpected(Error{context_result.error().code,
                                             context_result.error().message,
                                             context_result.error().location});
    }
    backend->m_vulkan_context = std::move(context_result.value());

    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(window, &width, &height)) {
        return nonstd::make_unexpected(
            Error{ErrorCode::unknown_error,
                  "SDL_GetWindowSizeInPixels failed: " + std::string(SDL_GetError())});
    }

    GOGGLES_TRY(backend->m_render_output.create_swapchain(
        backend->m_vulkan_context, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
        vk::Format::eB8G8R8A8Srgb));
    GOGGLES_TRY(backend->m_render_output.create_command_resources(backend->m_vulkan_context));
    GOGGLES_TRY(backend->m_render_output.create_sync_objects(backend->m_vulkan_context));
    backend->initialize_settings(settings);
    GOGGLES_TRY(backend->init_filter_chain());

    GOGGLES_LOG_INFO("Vulkan backend initialized: {}x{}", width, height);
    return {std::move(backend)};
}

auto VulkanBackend::create_headless(bool enable_validation, const std::filesystem::path& cache_dir,
                                    const RenderSettings& settings) -> ResultPtr<VulkanBackend> {
    GOGGLES_PROFILE_FUNCTION();

    auto backend = std::unique_ptr<VulkanBackend>(new VulkanBackend());
    backend->initialize_paths(cache_dir);

    auto context_result =
        backend_internal::VulkanContext::create_headless(enable_validation, settings.gpu_selector);
    if (!context_result) {
        return nonstd::make_unexpected(Error{context_result.error().code,
                                             context_result.error().message,
                                             context_result.error().location});
    }
    backend->m_vulkan_context = std::move(context_result.value());

    GOGGLES_TRY(backend->m_render_output.create_command_resources(backend->m_vulkan_context));
    GOGGLES_TRY(backend->m_render_output.create_sync_objects_headless(backend->m_vulkan_context));
    GOGGLES_TRY(backend->m_render_output.create_offscreen_image(
        backend->m_vulkan_context, vk::Extent2D{settings.source_width, settings.source_height}));
    backend->initialize_settings(settings);
    GOGGLES_TRY(backend->init_filter_chain());

    GOGGLES_LOG_INFO("Vulkan headless backend initialized: {}x{}",
                     backend->m_render_output.offscreen_extent.width,
                     backend->m_render_output.offscreen_extent.height);
    return {std::move(backend)};
}

void VulkanBackend::shutdown() {
    m_filter_chain_controller.shutdown([this]() {
        if (!m_vulkan_context.initialized()) {
            return;
        }

        auto wait_result = m_vulkan_context.device.waitIdle();
        if (wait_result != vk::Result::eSuccess) {
            GOGGLES_LOG_WARN("waitIdle failed during shutdown: {}", vk::to_string(wait_result));
        }
    });

    m_external_frame_importer.destroy(m_vulkan_context);
    m_render_output.destroy(m_vulkan_context);

    m_vulkan_context.destroy();

    GOGGLES_LOG_INFO("Vulkan backend shutdown");
}

auto VulkanBackend::recreate_swapchain(uint32_t width, uint32_t height, vk::Format source_format)
    -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    if (width == 0 || height == 0) {
        return make_error<void>(ErrorCode::unknown_error, "Swapchain size is zero");
    }

    vk::Format target_format = m_render_output.swapchain_format;
    bool recreate_filter_chain = false;
    if (source_format != vk::Format::eUndefined) {
        target_format = get_matching_swapchain_format(source_format);
        recreate_filter_chain = target_format != m_render_output.swapchain_format;
        if (recreate_filter_chain) {
            GOGGLES_LOG_INFO("Source format changed to {}, recreating swapchain with {}",
                             vk::to_string(source_format), vk::to_string(target_format));
        }
    }

    VK_TRY(m_vulkan_context.device.waitIdle(), ErrorCode::vulkan_device_lost,
           "waitIdle failed before swapchain recreation");

    m_render_output.cleanup_swapchain(m_vulkan_context);

    GOGGLES_TRY(m_render_output.create_swapchain(m_vulkan_context, width, height, target_format));

    if (recreate_filter_chain) {
        if (!m_filter_chain_controller.has_filter_chain()) {
            GOGGLES_TRY(init_filter_chain());
        } else {
            GOGGLES_TRY(m_filter_chain_controller.retarget_filter_chain(
                backend_internal::FilterChainController::OutputTarget{
                    .format = target_format,
                    .extent = m_render_output.target_extent(),
                }));
        }
    } else if (m_filter_chain_controller.has_filter_chain()) {
        GOGGLES_TRY(m_filter_chain_controller.handle_resize(m_render_output.target_extent()));
    }

    m_render_output.clear_resize_request();
    GOGGLES_LOG_DEBUG("Swapchain recreated: {}x{}", width, height);
    return {};
}

void VulkanBackend::wait_all_frames() {
    m_render_output.wait_all_frames(m_vulkan_context);
}

auto VulkanBackend::get_matching_swapchain_format(vk::Format source_format) -> vk::Format {
    if (is_srgb_format(source_format)) {
        return vk::Format::eB8G8R8A8Srgb;
    }
    return vk::Format::eB8G8R8A8Unorm;
}

auto VulkanBackend::is_srgb_format(vk::Format format) -> bool {
    switch (format) {
    case vk::Format::eR8Srgb:
    case vk::Format::eR8G8Srgb:
    case vk::Format::eR8G8B8Srgb:
    case vk::Format::eB8G8R8Srgb:
    case vk::Format::eR8G8B8A8Srgb:
    case vk::Format::eB8G8R8A8Srgb:
    case vk::Format::eA8B8G8R8SrgbPack32:
        return true;
    default:
        return false;
    }
}

auto VulkanBackend::init_filter_chain() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    return m_filter_chain_controller.recreate_filter_chain(make_filter_chain_build_config());
}

void VulkanBackend::load_shader_preset(const std::filesystem::path& preset_path) {
    m_filter_chain_controller.load_shader_preset(preset_path, [this]() { wait_all_frames(); });
}

void VulkanBackend::set_prechain_resolution(uint32_t width, uint32_t height) {
    m_filter_chain_controller.set_prechain_resolution(
        backend_internal::FilterChainController::PrechainResolutionConfig{
            .requested_resolution = vk::Extent2D{width, height},
        },
        [this]() { wait_all_frames(); });
}

auto VulkanBackend::get_prechain_resolution() const -> vk::Extent2D {
    return m_filter_chain_controller.current_prechain_resolution();
}

auto VulkanBackend::list_filter_controls() const
    -> std::vector<goggles::fc::FilterControlDescriptor> {
    return m_filter_chain_controller.list_filter_controls();
}

auto VulkanBackend::list_filter_controls(goggles::fc::FilterControlStage stage) const
    -> std::vector<goggles::fc::FilterControlDescriptor> {
    return m_filter_chain_controller.list_filter_controls(stage);
}

auto VulkanBackend::set_filter_control_value(goggles::fc::FilterControlId control_id, float value)
    -> bool {
    return m_filter_chain_controller.set_filter_control_value(control_id, value);
}

auto VulkanBackend::reset_filter_control_value(goggles::fc::FilterControlId control_id) -> bool {
    return m_filter_chain_controller.reset_filter_control_value(control_id);
}

void VulkanBackend::reset_filter_controls() {
    m_filter_chain_controller.reset_filter_controls();
}

auto VulkanBackend::record_render_commands(vk::CommandBuffer cmd, uint32_t image_index,
                                           const UiRenderCallback& ui_callback) -> Result<void> {
    GOGGLES_PROFILE_SCOPE("RecordCommands");

    if (!m_filter_chain_controller.has_filter_chain()) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }

    const auto imported_source = m_external_frame_importer.current_source();

    VK_TRY(cmd.reset(), ErrorCode::vulkan_device_lost, "Command buffer reset failed");

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    VK_TRY(cmd.begin(begin_info), ErrorCode::vulkan_device_lost, "Command buffer begin failed");

    vk::ImageMemoryBarrier src_barrier{};
    src_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    src_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    src_barrier.oldLayout = vk::ImageLayout::eUndefined;
    src_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.image = imported_source.image;
    src_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    src_barrier.subresourceRange.baseMipLevel = 0;
    src_barrier.subresourceRange.levelCount = 1;
    src_barrier.subresourceRange.baseArrayLayer = 0;
    src_barrier.subresourceRange.layerCount = 1;

    vk::ImageMemoryBarrier dst_barrier{};
    dst_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    dst_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    dst_barrier.oldLayout = vk::ImageLayout::eUndefined;
    dst_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    dst_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dst_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dst_barrier.image = m_render_output.target_image(image_index);
    dst_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    dst_barrier.subresourceRange.baseMipLevel = 0;
    dst_barrier.subresourceRange.levelCount = 1;
    dst_barrier.subresourceRange.baseArrayLayer = 0;
    dst_barrier.subresourceRange.layerCount = 1;

    std::array barriers = {src_barrier, dst_barrier};
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                        vk::PipelineStageFlagBits::eFragmentShader |
                            vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        {}, {}, {}, barriers);

    const auto integer_scale = resolve_record_integer_scale(
        m_scale_mode, m_integer_scale, imported_source.extent, m_render_output.target_extent());
    GOGGLES_TRY(
        m_filter_chain_controller.record(backend_internal::FilterChainController::RecordParams{
            .command_buffer = cmd,
            .source_image = imported_source.image,
            .source_view = imported_source.view,
            .source_width = imported_source.extent.width,
            .source_height = imported_source.extent.height,
            .target_view = m_render_output.target_view(image_index),
            .target_width = m_render_output.target_extent().width,
            .target_height = m_render_output.target_extent().height,
            .frame_index = m_render_output.current_frame_slot(),
            .scale_mode = to_fc_scale_mode(m_scale_mode),
            .integer_scale = integer_scale,
        }));

    if (ui_callback) {
        ui_callback(cmd, m_render_output.target_view(image_index), m_render_output.target_extent());
    }

    dst_barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    dst_barrier.dstAccessMask = vk::AccessFlagBits::eNone;
    dst_barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    dst_barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, dst_barrier);

    VK_TRY(cmd.end(), ErrorCode::vulkan_device_lost, "Command buffer end failed");

    return {};
}

auto VulkanBackend::record_clear_commands(vk::CommandBuffer cmd, uint32_t image_index,
                                          const UiRenderCallback& ui_callback) -> Result<void> {
    VK_TRY(cmd.reset(), ErrorCode::vulkan_device_lost, "Command buffer reset failed");

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    VK_TRY(cmd.begin(begin_info), ErrorCode::vulkan_device_lost, "Command buffer begin failed");

    vk::ImageMemoryBarrier barrier{};
    barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_render_output.target_image(image_index);
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                        vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {}, barrier);

    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.imageView = m_render_output.target_view(image_index);
    color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.clearValue.color = vk::ClearColorValue{std::array{0.0F, 0.0F, 0.0F, 1.0F}};

    vk::RenderingInfo rendering_info{};
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    rendering_info.renderArea.extent = m_render_output.target_extent();
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    cmd.beginRendering(rendering_info);
    cmd.endRendering();

    if (ui_callback) {
        ui_callback(cmd, m_render_output.target_view(image_index), m_render_output.target_extent());
    }

    barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eNone;
    barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, barrier);

    VK_TRY(cmd.end(), ErrorCode::vulkan_device_lost, "Command buffer end failed");

    return {};
}

auto VulkanBackend::render(const util::ExternalImageFrame* frame,
                           const UiRenderCallback& ui_callback) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    if (!m_vulkan_context.initialized()) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Backend not initialized");
    }
    if (!m_filter_chain_controller.has_filter_chain()) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }
    m_filter_chain_controller.advance_frame();
    m_filter_chain_controller.check_pending_chain_swap([this]() { wait_all_frames(); });
    m_filter_chain_controller.cleanup_retired_adapters();

    if (m_render_output.is_headless()) {
        auto cmd = GOGGLES_TRY(m_render_output.prepare_headless_frame(m_vulkan_context));
        m_external_frame_importer.retire_wait_semaphore(m_vulkan_context, 0);
        VK_TRY(cmd.reset(), ErrorCode::vulkan_device_lost, "Command buffer reset failed");

        vk::CommandBufferBeginInfo begin_info{};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        VK_TRY(cmd.begin(begin_info), ErrorCode::vulkan_device_lost, "Command buffer begin failed");

        if (frame) {
            const auto imported_source = GOGGLES_TRY(
                m_external_frame_importer.import_external_image(m_vulkan_context, frame->image));

            vk::ImageMemoryBarrier src_barrier{};
            src_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            src_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            src_barrier.oldLayout = vk::ImageLayout::eUndefined;
            src_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            src_barrier.image = imported_source.image;
            src_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            src_barrier.subresourceRange.levelCount = 1;
            src_barrier.subresourceRange.layerCount = 1;

            vk::ImageMemoryBarrier dst_barrier{};
            dst_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            dst_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            dst_barrier.oldLayout = vk::ImageLayout::eUndefined;
            dst_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            dst_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            dst_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            dst_barrier.image = m_render_output.target_image();
            dst_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            dst_barrier.subresourceRange.levelCount = 1;
            dst_barrier.subresourceRange.layerCount = 1;

            std::array barriers = {src_barrier, dst_barrier};
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                vk::PipelineStageFlagBits::eFragmentShader |
                                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                {}, {}, {}, barriers);

            const auto integer_scale =
                resolve_record_integer_scale(m_scale_mode, m_integer_scale, imported_source.extent,
                                             m_render_output.target_extent());
            GOGGLES_TRY(m_filter_chain_controller.record(
                backend_internal::FilterChainController::RecordParams{
                    .command_buffer = cmd,
                    .source_image = imported_source.image,
                    .source_view = imported_source.view,
                    .source_width = imported_source.extent.width,
                    .source_height = imported_source.extent.height,
                    .target_view = m_render_output.target_view(),
                    .target_width = m_render_output.target_extent().width,
                    .target_height = m_render_output.target_extent().height,
                    .frame_index = 0,
                    .scale_mode = to_fc_scale_mode(m_scale_mode),
                    .integer_scale = integer_scale,
                }));
        } else {
            vk::ImageMemoryBarrier barrier{};
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            barrier.oldLayout = vk::ImageLayout::eUndefined;
            barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = m_render_output.target_image();
            barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;

            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
                                barrier);

            vk::RenderingAttachmentInfo color_attachment{};
            color_attachment.imageView = m_render_output.target_view();
            color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
            color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
            color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
            color_attachment.clearValue.color =
                vk::ClearColorValue{std::array{0.0F, 0.0F, 0.0F, 1.0F}};

            vk::RenderingInfo rendering_info{};
            rendering_info.renderArea.offset = vk::Offset2D{0, 0};
            rendering_info.renderArea.extent = m_render_output.target_extent();
            rendering_info.layerCount = 1;
            rendering_info.colorAttachmentCount = 1;
            rendering_info.pColorAttachments = &color_attachment;

            cmd.beginRendering(rendering_info);
            cmd.endRendering();
        }

        VK_TRY(cmd.end(), ErrorCode::vulkan_device_lost, "Command buffer end failed");
        if (frame && frame->sync_fd.valid()) {
            m_external_frame_importer.prepare_wait_semaphore(m_vulkan_context, frame->sync_fd, 0);
        }

        auto submit_result = m_render_output.submit_headless(
            m_vulkan_context, m_external_frame_importer.wait_semaphore(0),
            backend_internal::ExternalFrameImporter::WAIT_STAGE);
        if (!submit_result) {
            m_external_frame_importer.retire_wait_semaphore(m_vulkan_context, 0);
            return submit_result;
        }
        return {};
    }

    uint32_t image_index = GOGGLES_TRY(m_render_output.acquire_next_image(m_vulkan_context));
    const uint32_t frame_slot = m_render_output.current_frame;
    m_external_frame_importer.retire_wait_semaphore(m_vulkan_context, frame_slot);

    if (frame) {
        GOGGLES_TRY(
            m_external_frame_importer.import_external_image(m_vulkan_context, frame->image));
        GOGGLES_TRY(
            record_render_commands(m_render_output.command_buffer(), image_index, ui_callback));
        if (frame->sync_fd.valid()) {
            m_external_frame_importer.prepare_wait_semaphore(m_vulkan_context, frame->sync_fd,
                                                             frame_slot);
        }
    } else {
        GOGGLES_TRY(
            record_clear_commands(m_render_output.command_buffer(), image_index, ui_callback));
    }

    auto submit_result = m_render_output.submit_and_present(
        m_vulkan_context, image_index, m_external_frame_importer.wait_semaphore(frame_slot),
        backend_internal::ExternalFrameImporter::WAIT_STAGE);
    if (!submit_result) {
        m_external_frame_importer.retire_wait_semaphore(m_vulkan_context, frame_slot);
        return submit_result;
    }
    return {};
}

auto VulkanBackend::readback_to_png(const std::filesystem::path& output) -> Result<void> {
    return m_render_output.readback_to_png(m_vulkan_context, output);
}

auto VulkanBackend::reload_shader_preset(const std::filesystem::path& preset_path) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    if (!m_vulkan_context.initialized()) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Backend not initialized");
    }

    return m_filter_chain_controller.reload_shader_preset(preset_path,
                                                          make_filter_chain_build_config());
}

void VulkanBackend::set_filter_chain_policy(const FilterChainStagePolicy& policy) {
    m_filter_chain_controller.set_stage_policy(policy.prechain_enabled, policy.effect_stage_enabled,
                                               [this]() { wait_all_frames(); });
}

auto VulkanBackend::make_filter_chain_build_config() const
    -> backend_internal::FilterChainController::AdapterBuildConfig {
    return backend_internal::FilterChainController::AdapterBuildConfig{
        .device_info =
            backend_internal::FilterChainController::VulkanDeviceInfo{
                .physical_device = m_vulkan_context.physical_device,
                .device = m_vulkan_context.device,
                .graphics_queue = m_vulkan_context.graphics_queue,
                .graphics_queue_family_index = m_vulkan_context.graphics_queue_family,
                .cache_dir = m_cache_dir.string(),
            },
        .chain_config =
            backend_internal::FilterChainController::ChainConfig{
                .target_format = static_cast<VkFormat>(m_render_output.swapchain_format),
                .frames_in_flight = backend_internal::RenderOutput::MAX_FRAMES_IN_FLIGHT,
                .initial_prechain_width =
                    m_filter_chain_controller.current_prechain_resolution().width,
                .initial_prechain_height =
                    m_filter_chain_controller.current_prechain_resolution().height,
            },
    };
}

auto VulkanBackend::current_filter_target_extent() const -> vk::Extent2D {
    return m_render_output.target_extent();
}

} // namespace goggles::render
