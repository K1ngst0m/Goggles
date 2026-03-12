#include "chain_runtime.hpp"

#include "chain_builder.hpp"
#include "vulkan_result.hpp"

#include <algorithm>
#include <cstring>
#include <render/shader/shader_runtime.hpp>
#include <util/diagnostics/log_sink.hpp>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render {

namespace {

struct ReadbackStagingBuffer {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    bool is_coherent = false;
};

auto capture_mode_name(diagnostics::CaptureMode mode) -> std::string {
    switch (mode) {
    case diagnostics::CaptureMode::minimal:
        return "minimal";
    case diagnostics::CaptureMode::investigate:
        return "investigate";
    case diagnostics::CaptureMode::forensic:
        return "forensic";
    case diagnostics::CaptureMode::standard:
    default:
        return "standard";
    }
}

void emit_timestamp_event(diagnostics::DiagnosticSession* session, diagnostics::Severity severity,
                          std::string message) {
    if (session == nullptr) {
        return;
    }

    diagnostics::DiagnosticEvent event{};
    event.severity = severity;
    event.original_severity = severity;
    event.category = diagnostics::Category::runtime;
    event.localization = {.pass_ordinal = diagnostics::LocalizationKey::CHAIN_LEVEL,
                          .stage = "timestamp",
                          .resource = {}};
    event.frame_index = session->current_frame();
    event.message = std::move(message);
    session->emit(std::move(event));
}

auto create_readback_staging_buffer(vk::Device device, vk::PhysicalDevice physical_device,
                                    vk::DeviceSize size) -> Result<ReadbackStagingBuffer> {
    vk::BufferCreateInfo buffer_info{};
    buffer_info.size = size;
    buffer_info.usage = vk::BufferUsageFlagBits::eTransferDst;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;

    const auto [buffer_result, buffer] = device.createBuffer(buffer_info);
    if (buffer_result != vk::Result::eSuccess) {
        return make_error<ReadbackStagingBuffer>(ErrorCode::vulkan_init_failed,
                                                 "Failed to create capture staging buffer: " +
                                                     vk::to_string(buffer_result));
    }

    const auto requirements = device.getBufferMemoryRequirements(buffer);
    const auto mem_props = physical_device.getMemoryProperties();

    uint32_t memory_type_index = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        const auto flags = mem_props.memoryTypes[i].propertyFlags;
        if ((requirements.memoryTypeBits & (1U << i)) == 0U) {
            continue;
        }
        if ((flags & (vk::MemoryPropertyFlagBits::eHostVisible |
                      vk::MemoryPropertyFlagBits::eHostCoherent)) ==
            (vk::MemoryPropertyFlagBits::eHostVisible |
             vk::MemoryPropertyFlagBits::eHostCoherent)) {
            memory_type_index = i;
            break;
        }
    }

    if (memory_type_index == UINT32_MAX) {
        device.destroyBuffer(buffer);
        return make_error<ReadbackStagingBuffer>(ErrorCode::vulkan_init_failed,
                                                 "No host-visible capture staging memory type");
    }

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = requirements.size;
    alloc_info.memoryTypeIndex = memory_type_index;

    const auto [alloc_result, memory] = device.allocateMemory(alloc_info);
    if (alloc_result != vk::Result::eSuccess) {
        device.destroyBuffer(buffer);
        return make_error<ReadbackStagingBuffer>(ErrorCode::vulkan_init_failed,
                                                 "Failed to allocate capture staging memory: " +
                                                     vk::to_string(alloc_result));
    }

    const auto bind_result = device.bindBufferMemory(buffer, memory, 0);
    if (bind_result != vk::Result::eSuccess) {
        device.freeMemory(memory);
        device.destroyBuffer(buffer);
        return make_error<ReadbackStagingBuffer>(ErrorCode::vulkan_init_failed,
                                                 "Failed to bind capture staging memory: " +
                                                     vk::to_string(bind_result));
    }

    return ReadbackStagingBuffer{
        .buffer = buffer,
        .memory = memory,
        .is_coherent = (mem_props.memoryTypes[memory_type_index].propertyFlags &
                        vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags{},
    };
}

void destroy_readback_staging_buffer(vk::Device device, ReadbackStagingBuffer& staging) {
    if (staging.memory) {
        device.freeMemory(staging.memory);
        staging.memory = nullptr;
    }
    if (staging.buffer) {
        device.destroyBuffer(staging.buffer);
        staging.buffer = nullptr;
    }
}

auto capture_image_pixels(const VulkanContext& vk_ctx, vk::Image image, vk::Extent2D extent,
                          vk::ImageLayout current_layout) -> Result<CapturedImage> {
    if (extent.width == 0 || extent.height == 0) {
        return make_error<CapturedImage>(ErrorCode::invalid_data,
                                         "Cannot capture zero-sized image");
    }

    const vk::DeviceSize buffer_size =
        static_cast<vk::DeviceSize>(extent.width) * extent.height * 4;
    auto staging = GOGGLES_TRY(
        create_readback_staging_buffer(vk_ctx.device, vk_ctx.physical_device, buffer_size));

    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.commandPool = vk_ctx.command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = 1;

    const auto [alloc_result, command_buffers] = vk_ctx.device.allocateCommandBuffers(alloc_info);
    if (alloc_result != vk::Result::eSuccess || command_buffers.empty()) {
        destroy_readback_staging_buffer(vk_ctx.device, staging);
        return make_error<CapturedImage>(ErrorCode::vulkan_init_failed,
                                         "Failed to allocate capture command buffer: " +
                                             vk::to_string(alloc_result));
    }
    auto command_buffer = command_buffers.front();

    vk::FenceCreateInfo fence_info{};
    const auto [fence_result, fence] = vk_ctx.device.createFence(fence_info);
    if (fence_result != vk::Result::eSuccess) {
        vk_ctx.device.freeCommandBuffers(vk_ctx.command_pool, command_buffer);
        destroy_readback_staging_buffer(vk_ctx.device, staging);
        return make_error<CapturedImage>(ErrorCode::vulkan_init_failed,
                                         "Failed to create capture fence: " +
                                             vk::to_string(fence_result));
    }

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    VK_TRY(command_buffer.begin(begin_info), ErrorCode::vulkan_device_lost,
           "Capture command buffer begin failed");

    vk::ImageMemoryBarrier to_transfer{};
    to_transfer.srcAccessMask = vk::AccessFlagBits::eShaderRead |
                                vk::AccessFlagBits::eColorAttachmentWrite |
                                vk::AccessFlagBits::eTransferWrite;
    to_transfer.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    to_transfer.oldLayout = current_layout;
    to_transfer.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image = image;
    to_transfer.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                                   vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, to_transfer);

    vk::BufferImageCopy region{};
    region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    region.imageExtent = vk::Extent3D{extent.width, extent.height, 1};
    command_buffer.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, staging.buffer,
                                     region);

    vk::ImageMemoryBarrier restore{};
    restore.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    restore.dstAccessMask =
        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eColorAttachmentWrite;
    restore.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    restore.newLayout = current_layout;
    restore.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    restore.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    restore.image = image;
    restore.subresourceRange = to_transfer.subresourceRange;

    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, restore);

    VK_TRY(command_buffer.end(), ErrorCode::vulkan_device_lost,
           "Capture command buffer end failed");

    vk::SubmitInfo submit_info{};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    VK_TRY(vk_ctx.graphics_queue.submit(submit_info, fence), ErrorCode::vulkan_device_lost,
           "Capture queue submit failed");
    VK_TRY(vk_ctx.device.waitForFences(fence, VK_TRUE, UINT64_MAX), ErrorCode::vulkan_device_lost,
           "Capture fence wait failed");

    const auto [map_result, data] = vk_ctx.device.mapMemory(staging.memory, 0, buffer_size);
    if (map_result != vk::Result::eSuccess) {
        vk_ctx.device.destroyFence(fence);
        vk_ctx.device.freeCommandBuffers(vk_ctx.command_pool, command_buffer);
        destroy_readback_staging_buffer(vk_ctx.device, staging);
        return make_error<CapturedImage>(ErrorCode::vulkan_device_lost,
                                         "Failed to map capture staging memory: " +
                                             vk::to_string(map_result));
    }

    if (!staging.is_coherent) {
        vk::MappedMemoryRange range{};
        range.memory = staging.memory;
        range.offset = 0;
        range.size = VK_WHOLE_SIZE;
        const auto invalidate_result = vk_ctx.device.invalidateMappedMemoryRanges(range);
        if (invalidate_result != vk::Result::eSuccess) {
            GOGGLES_LOG_WARN("invalidateMappedMemoryRanges failed during capture: {}",
                             vk::to_string(invalidate_result));
        }
    }

    CapturedImage captured{};
    captured.width = extent.width;
    captured.height = extent.height;
    captured.rgba.resize(static_cast<size_t>(buffer_size));
    std::memcpy(captured.rgba.data(), data, static_cast<size_t>(buffer_size));

    vk_ctx.device.unmapMemory(staging.memory);
    vk_ctx.device.destroyFence(fence);
    vk_ctx.device.freeCommandBuffers(vk_ctx.command_pool, command_buffer);
    destroy_readback_staging_buffer(vk_ctx.device, staging);

    return captured;
}

} // namespace

auto ChainRuntime::create(const VulkanContext& vk_ctx, vk::Format swapchain_format,
                          uint32_t num_sync_indices, const FilterChainPaths& paths,
                          vk::Extent2D source_resolution) -> ResultPtr<ChainRuntime> {
    auto chain = std::unique_ptr<ChainRuntime>(new ChainRuntime());

    chain->m_shader_runtime = GOGGLES_TRY(ShaderRuntime::create(paths.cache_dir));
    chain->m_resources = GOGGLES_TRY(
        ChainResources::create(vk_ctx, swapchain_format, num_sync_indices, *chain->m_shader_runtime,
                               paths.shader_dir, source_resolution));
    chain->set_stage_policy(true, true);

    return make_result_ptr(std::move(chain));
}

ChainRuntime::~ChainRuntime() {
    shutdown();
}

void ChainRuntime::shutdown() {
    if (m_resources) {
        m_resources->shutdown();
        m_resources.reset();
    }
    if (m_shader_runtime) {
        m_shader_runtime->shutdown();
        m_shader_runtime.reset();
    }
}

auto ChainRuntime::load_preset(const std::filesystem::path& preset_path) -> Result<void> {
    if (!m_resources) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }
    if (preset_path.empty()) {
        return {};
    }

    auto compiled = GOGGLES_TRY(ChainBuilder::build(
        m_resources->m_vk_ctx, *m_resources->m_shader_runtime, m_resources->m_num_sync_indices,
        *m_resources->m_texture_loader, preset_path, m_diagnostic_session.get()));
    m_resources->install(std::move(compiled), m_diagnostic_session.get());
    sync_gpu_timestamp_pool();
    m_controls.replay_values(*m_resources);
    return {};
}

auto ChainRuntime::handle_resize(vk::Extent2D new_viewport_extent) -> Result<void> {
    if (!m_resources) {
        return {};
    }
    return m_resources->handle_resize(new_viewport_extent);
}

void ChainRuntime::record(vk::CommandBuffer cmd, vk::Image original_image,
                          vk::ImageView original_view, vk::Extent2D original_extent,
                          vk::ImageView target_view, vk::Extent2D viewport_extent,
                          uint32_t frame_index, ScaleMode scale_mode, uint32_t integer_scale) {
    if (!m_resources) {
        return;
    }
    if (m_diagnostic_session) {
        m_diagnostic_session->begin_frame(frame_index);
    }
    m_executor.record(*m_resources, cmd, original_image, original_view, original_extent,
                      target_view, viewport_extent, frame_index, scale_mode, integer_scale,
                      m_diagnostic_session.get(), m_gpu_timestamp_pool.get());
    if (m_diagnostic_session) {
        m_diagnostic_session->end_frame();
    }
}

void ChainRuntime::set_stage_policy(bool prechain_enabled, bool effect_stage_enabled) {
    m_prechain_policy_enabled = prechain_enabled;
    m_effect_stage_policy_enabled = effect_stage_enabled;

    if (!m_resources) {
        return;
    }

    m_resources->set_prechain_enabled(m_prechain_policy_enabled);
    m_resources->set_bypass(!m_effect_stage_policy_enabled);
}

void ChainRuntime::set_prechain_resolution(vk::Extent2D resolution) {
    if (!m_resources) {
        return;
    }
    m_resources->set_prechain_resolution(resolution.width, resolution.height);
}

auto ChainRuntime::get_prechain_resolution() const -> vk::Extent2D {
    if (!m_resources) {
        return vk::Extent2D{};
    }
    return m_resources->get_prechain_resolution();
}

auto ChainRuntime::list_controls() const -> std::vector<FilterControlDescriptor> {
    if (!m_resources) {
        return {};
    }
    return m_controls.list_controls(*m_resources);
}

auto ChainRuntime::list_controls(FilterControlStage stage) const
    -> std::vector<FilterControlDescriptor> {
    if (!m_resources) {
        return {};
    }
    return m_controls.list_controls(*m_resources, stage);
}

auto ChainRuntime::set_control_value(FilterControlId control_id, float value) -> bool {
    if (!m_resources) {
        return false;
    }
    return m_controls.set_control_value(*m_resources, control_id, value);
}

auto ChainRuntime::reset_control_value(FilterControlId control_id) -> bool {
    if (!m_resources) {
        return false;
    }
    return m_controls.reset_control_value(*m_resources, control_id);
}

void ChainRuntime::reset_controls() {
    if (!m_resources) {
        return;
    }
    m_controls.reset_controls(*m_resources);
}

void ChainRuntime::create_diagnostic_session(diagnostics::DiagnosticPolicy policy) {
    m_diagnostic_session = diagnostics::DiagnosticSession::create(policy);
    auto identity = m_diagnostic_session->identity();
    identity.capture_mode = capture_mode_name(policy.capture_mode);
    m_diagnostic_session->update_identity(std::move(identity));
    m_diagnostic_session->register_sink(std::make_unique<diagnostics::LogSink>());
    sync_gpu_timestamp_pool();
}

void ChainRuntime::destroy_diagnostic_session() {
    m_gpu_timestamp_pool.reset();
    m_diagnostic_session.reset();
}

void ChainRuntime::sync_gpu_timestamp_pool() {
    m_gpu_timestamp_pool.reset();

    if (!m_diagnostic_session || !m_resources) {
        return;
    }

    if (m_diagnostic_session->policy().tier < diagnostics::ActivationTier::tier1) {
        return;
    }

    std::unique_ptr<diagnostics::GpuTimestampPool> pool;
    if (m_diagnostic_session->policy().gpu_timestamp_availability ==
        diagnostics::GpuTimestampAvailabilityMode::force_unavailable) {
        pool = diagnostics::GpuTimestampPool::create_unavailable();
    } else {
        auto pool_result = diagnostics::GpuTimestampPool::create(
            m_resources->m_vk_ctx.device, m_resources->m_vk_ctx.physical_device,
            diagnostics::GpuTimestampPoolCreateInfo{
                .graphics_queue_family_index = m_resources->m_vk_ctx.graphics_queue_family_index,
                .max_passes =
                    static_cast<uint32_t>(std::max<size_t>(m_resources->pass_count(), 1U)),
                .frames_in_flight = m_resources->m_num_sync_indices});
        if (!pool_result) {
            emit_timestamp_event(m_diagnostic_session.get(), diagnostics::Severity::warning,
                                 "Failed to enable GPU timestamps: " + pool_result.error().message);
            return;
        }

        pool = std::move(*pool_result);
    }

    if (!pool->is_available()) {
        emit_timestamp_event(m_diagnostic_session.get(), diagnostics::Severity::info,
                             "GPU timestamps are unavailable on this device");
    }

    m_gpu_timestamp_pool = std::move(pool);
}

auto ChainRuntime::diagnostic_session() -> diagnostics::DiagnosticSession* {
    return m_diagnostic_session.get();
}

auto ChainRuntime::diagnostic_session() const -> const diagnostics::DiagnosticSession* {
    return m_diagnostic_session.get();
}

auto ChainRuntime::capture_pass_output(uint32_t pass_ordinal) const -> Result<CapturedImage> {
    if (!m_resources) {
        return make_error<CapturedImage>(ErrorCode::vulkan_init_failed,
                                         "Filter chain not initialized");
    }
    if (pass_ordinal >= m_resources->m_framebuffers.size() ||
        !m_resources->m_framebuffers[pass_ordinal]) {
        return make_error<CapturedImage>(ErrorCode::invalid_data,
                                         "Pass output is unavailable for ordinal " +
                                             std::to_string(pass_ordinal));
    }

    const auto& framebuffer = m_resources->m_framebuffers[pass_ordinal];
    return capture_image_pixels(m_resources->m_vk_ctx, framebuffer->image(), framebuffer->extent(),
                                vk::ImageLayout::eShaderReadOnlyOptimal);
}

} // namespace goggles::render
