#pragma once

#include "vulkan_context.hpp"

#include <array>
#include <util/error.hpp>
#include <util/external_image.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::render::backend_internal {

/// @brief Backend-owned DMA-BUF import state and temporary explicit-sync waits.
struct ExternalFrameImporter {
    static constexpr uint32_t MAX_FRAME_SLOTS = 2;
    static constexpr vk::PipelineStageFlags WAIT_STAGE = vk::PipelineStageFlagBits::eFragmentShader;

    struct ImportedImage {
        vk::Image image;
        vk::DeviceMemory memory;
        vk::ImageView view;
    };

    struct ImportedSource {
        vk::Image image;
        vk::ImageView view;
        vk::Extent2D extent;
        vk::Format format = vk::Format::eUndefined;
    };

    [[nodiscard]] auto import_external_image(VulkanContext& context,
                                             const ::goggles::util::ExternalImage& image)
        -> Result<ImportedSource>;
    void prepare_wait_semaphore(VulkanContext& context, const ::goggles::util::UniqueFd& sync_fd,
                                uint32_t frame_slot);
    void retire_wait_semaphore(VulkanContext& context, uint32_t frame_slot);
    void destroy(VulkanContext& context);
    void clear_current_source();

    [[nodiscard]] auto current_source() const -> ImportedSource;
    [[nodiscard]] auto wait_semaphore(uint32_t frame_slot) const -> vk::Semaphore;

    ImportedImage imported_image;
    vk::Extent2D import_extent;
    vk::Format source_format = vk::Format::eUndefined;
    std::array<vk::Semaphore, MAX_FRAME_SLOTS> pending_wait_semaphores{};
};

} // namespace goggles::render::backend_internal
