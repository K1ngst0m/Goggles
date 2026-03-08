#include "external_frame_importer.hpp"

#include <format>
#include <util/logging.hpp>

namespace goggles::render::backend_internal {

namespace {

// from drm_fourcc.h (kept local to avoid adding extra headers)
constexpr uint64_t DRM_FORMAT_MOD_INVALID = 0xffffffffffffffULL;

auto find_memory_type(const vk::PhysicalDeviceMemoryProperties& mem_props, uint32_t type_bits)
    -> uint32_t {
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if (type_bits & (1U << i)) {
            return i;
        }
    }
    return UINT32_MAX;
}

struct DmabufImageCreateChain {
    vk::ExternalMemoryImageCreateInfo ext_mem_info;
    vk::ImageDrmFormatModifierExplicitCreateInfoEXT modifier_info;
    vk::SubresourceLayout plane_layout;
    vk::ImageCreateInfo image_info;
};

void init_dmabuf_image_create_chain(const ::goggles::util::ExternalImage& frame,
                                    vk::Format vk_format, DmabufImageCreateChain* chain) {
    chain->ext_mem_info = vk::ExternalMemoryImageCreateInfo{};
    chain->modifier_info = vk::ImageDrmFormatModifierExplicitCreateInfoEXT{};
    chain->plane_layout = vk::SubresourceLayout{};
    chain->image_info = vk::ImageCreateInfo{};

    chain->ext_mem_info.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT;

    chain->plane_layout.offset = frame.offset;
    chain->plane_layout.size = 0;
    chain->plane_layout.rowPitch = frame.stride;
    chain->plane_layout.arrayPitch = 0;
    chain->plane_layout.depthPitch = 0;

    chain->modifier_info.drmFormatModifier = frame.modifier;
    chain->modifier_info.drmFormatModifierPlaneCount = 1;
    chain->modifier_info.pPlaneLayouts = &chain->plane_layout;

    chain->ext_mem_info.pNext = &chain->modifier_info;

    chain->image_info.pNext = &chain->ext_mem_info;
    chain->image_info.imageType = vk::ImageType::e2D;
    chain->image_info.format = vk_format;
    chain->image_info.extent = vk::Extent3D{frame.width, frame.height, 1};
    chain->image_info.mipLevels = 1;
    chain->image_info.arrayLayers = 1;
    chain->image_info.samples = vk::SampleCountFlagBits::e1;
    chain->image_info.tiling = vk::ImageTiling::eDrmFormatModifierEXT;
    chain->image_info.usage =
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;
    chain->image_info.sharingMode = vk::SharingMode::eExclusive;
    chain->image_info.initialLayout = vk::ImageLayout::eUndefined;
}

auto get_dmabuf_memory_type_bits(vk::Device device, int fd) -> Result<uint32_t> {
    VkMemoryFdPropertiesKHR fd_props_raw{};
    fd_props_raw.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;

    auto fd_props_result =
        static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkGetMemoryFdPropertiesKHR(
            device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd, &fd_props_raw));
    if (fd_props_result != vk::Result::eSuccess) {
        return make_error<uint32_t>(ErrorCode::vulkan_init_failed,
                                    "Stale DMA-BUF fd, skipping frame");
    }

    return fd_props_raw.memoryTypeBits;
}

auto allocate_imported_dmabuf_memory(vk::Device device, vk::Image image, vk::DeviceSize size,
                                     uint32_t mem_type_index, ::goggles::util::UniqueFd import_fd,
                                     const vk::MemoryDedicatedRequirements& dedicated_reqs)
    -> Result<vk::DeviceMemory> {
    vk::ImportMemoryFdInfoKHR import_info{};
    import_info.handleType = vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT;
    import_info.fd = import_fd.get();

    vk::MemoryDedicatedAllocateInfo dedicated_alloc{};
    dedicated_alloc.image = image;
    if (dedicated_reqs.requiresDedicatedAllocation || dedicated_reqs.prefersDedicatedAllocation) {
        import_info.pNext = &dedicated_alloc;
    }

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.pNext = &import_info;
    alloc_info.allocationSize = size;
    alloc_info.memoryTypeIndex = mem_type_index;

    auto [alloc_result, memory] = device.allocateMemory(alloc_info);
    if (alloc_result != vk::Result::eSuccess) {
        return make_error<vk::DeviceMemory>(ErrorCode::vulkan_init_failed,
                                            "Failed to import DMA-BUF memory: " +
                                                vk::to_string(alloc_result));
    }

    import_fd.release();
    return memory;
}

auto create_imported_image_view(vk::Device device, vk::Image image, vk::Format format)
    -> Result<vk::ImageView> {
    vk::ImageViewCreateInfo view_info{};
    view_info.image = image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    auto [view_result, view] = device.createImageView(view_info);
    if (view_result != vk::Result::eSuccess) {
        return make_error<vk::ImageView>(ErrorCode::vulkan_init_failed,
                                         "Failed to create DMA-BUF image view: " +
                                             vk::to_string(view_result));
    }
    return view;
}

void destroy_imported_image(vk::Device device, ExternalFrameImporter& importer) {
    if (device) {
        if (importer.imported_image.view) {
            device.destroyImageView(importer.imported_image.view);
            importer.imported_image.view = nullptr;
        }
        if (importer.imported_image.image) {
            device.destroyImage(importer.imported_image.image);
            importer.imported_image.image = nullptr;
        }
        if (importer.imported_image.memory) {
            device.freeMemory(importer.imported_image.memory);
            importer.imported_image.memory = nullptr;
        }
        return;
    }

    importer.imported_image = {};
}

} // namespace

auto ExternalFrameImporter::import_external_image(VulkanContext& context,
                                                  const ::goggles::util::ExternalImage& image)
    -> Result<ExternalFrameImporter::ImportedSource> {
    auto& device = context.device;
    auto& physical_device = context.physical_device;

    if (!image.handle.valid()) {
        return make_error<ExternalFrameImporter::ImportedSource>(ErrorCode::vulkan_init_failed,
                                                                 "Invalid DMA-BUF fd");
    }

    if (image.modifier == DRM_FORMAT_MOD_INVALID) {
        return make_error<ExternalFrameImporter::ImportedSource>(
            ErrorCode::invalid_data,
            std::format("Invalid DMA-BUF modifier 0x{:x} "
                        "(DRM_FORMAT_MOD_INVALID); capture did not provide a valid modifier, "
                        "cannot import",
                        image.modifier));
    }

    const auto wait_result = device.waitIdle();
    if (wait_result != vk::Result::eSuccess) {
        return make_error<ExternalFrameImporter::ImportedSource>(
            ErrorCode::vulkan_device_lost,
            "waitIdle failed before reimport: " + vk::to_string(wait_result));
    }
    destroy_imported_image(device, *this);
    this->clear_current_source();

    const auto vk_format = image.format;
    DmabufImageCreateChain chain{};
    init_dmabuf_image_create_chain(image, vk_format, &chain);

    auto [img_result, imported_vk_image] = device.createImage(chain.image_info);
    if (img_result != vk::Result::eSuccess) {
        return make_error<ExternalFrameImporter::ImportedSource>(
            ErrorCode::vulkan_init_failed,
            std::format("Failed to create DMA-BUF image (format={}, modifier=0x{:x}): {}",
                        vk::to_string(vk_format), image.modifier, vk::to_string(img_result)));
    }
    imported_image.image = imported_vk_image;

    vk::ImageMemoryRequirementsInfo2 mem_reqs_info{};
    mem_reqs_info.image = imported_image.image;

    vk::MemoryDedicatedRequirements dedicated_reqs{};
    vk::MemoryRequirements2 mem_reqs2{};
    mem_reqs2.pNext = &dedicated_reqs;

    device.getImageMemoryRequirements2(&mem_reqs_info, &mem_reqs2);
    const auto mem_reqs = mem_reqs2.memoryRequirements;

    auto fd_type_bits_result = get_dmabuf_memory_type_bits(device, image.handle.get());
    if (!fd_type_bits_result) {
        destroy_imported_image(device, *this);
        return make_error<ExternalFrameImporter::ImportedSource>(
            fd_type_bits_result.error().code, fd_type_bits_result.error().message);
    }

    const auto mem_props = physical_device.getMemoryProperties();
    const uint32_t combined_bits = mem_reqs.memoryTypeBits & fd_type_bits_result.value();
    const uint32_t mem_type_index = find_memory_type(mem_props, combined_bits);

    if (mem_type_index == UINT32_MAX) {
        destroy_imported_image(device, *this);
        return make_error<ExternalFrameImporter::ImportedSource>(
            ErrorCode::vulkan_init_failed, "No suitable memory type for DMA-BUF import");
    }

    auto import_fd = image.handle.dup();
    if (!import_fd) {
        destroy_imported_image(device, *this);
        return make_error<ExternalFrameImporter::ImportedSource>(ErrorCode::vulkan_init_failed,
                                                                 "Failed to dup DMA-BUF fd");
    }

    auto mem_result =
        allocate_imported_dmabuf_memory(device, imported_image.image, mem_reqs.size, mem_type_index,
                                        std::move(import_fd), dedicated_reqs);
    if (!mem_result) {
        destroy_imported_image(device, *this);
        return make_error<ExternalFrameImporter::ImportedSource>(mem_result.error().code,
                                                                 mem_result.error().message);
    }
    imported_image.memory = mem_result.value();

    const auto bind_result = device.bindImageMemory(imported_image.image, imported_image.memory, 0);
    if (bind_result != vk::Result::eSuccess) {
        destroy_imported_image(device, *this);
        return make_error<ExternalFrameImporter::ImportedSource>(ErrorCode::vulkan_init_failed,
                                                                 "Failed to bind DMA-BUF memory: " +
                                                                     vk::to_string(bind_result));
    }

    auto view_result = create_imported_image_view(device, imported_image.image, vk_format);
    if (!view_result) {
        destroy_imported_image(device, *this);
        return make_error<ExternalFrameImporter::ImportedSource>(view_result.error().code,
                                                                 view_result.error().message);
    }
    imported_image.view = view_result.value();
    import_extent = vk::Extent2D{image.width, image.height};
    source_format = vk_format;

    GOGGLES_LOG_TRACE("DMA-BUF imported: {}x{}, format={}, modifier=0x{:x}", image.width,
                      image.height, vk::to_string(vk_format), image.modifier);
    return current_source();
}

void ExternalFrameImporter::prepare_wait_semaphore(VulkanContext& context,
                                                   const ::goggles::util::UniqueFd& sync_fd,
                                                   uint32_t frame_slot) {
    if (frame_slot >= pending_wait_semaphores.size() || !sync_fd.valid()) {
        return;
    }

    auto& device = context.device;
    auto import_fd = sync_fd.dup();
    if (!import_fd.valid()) {
        GOGGLES_LOG_WARN("Failed to dup sync_fd for temporary wait import");
        return;
    }

    vk::SemaphoreCreateInfo sem_info{};
    auto [sem_result, semaphore] = device.createSemaphore(sem_info);
    if (sem_result != vk::Result::eSuccess) {
        GOGGLES_LOG_WARN("Failed to create temporary wait semaphore: {}",
                         vk::to_string(sem_result));
        return;
    }

    vk::ImportSemaphoreFdInfoKHR import_info{};
    import_info.semaphore = semaphore;
    import_info.handleType = vk::ExternalSemaphoreHandleTypeFlagBits::eSyncFd;
    import_info.fd = import_fd.get();
    import_info.flags = vk::SemaphoreImportFlagBits::eTemporary;

    const auto import_result = device.importSemaphoreFdKHR(import_info);
    if (import_result != vk::Result::eSuccess) {
        device.destroySemaphore(semaphore);
        GOGGLES_LOG_WARN("Failed to import sync_fd as semaphore: {}", vk::to_string(import_result));
        return;
    }

    import_fd.release();
    pending_wait_semaphores[frame_slot] = semaphore;
}

void ExternalFrameImporter::retire_wait_semaphore(VulkanContext& context, uint32_t frame_slot) {
    if (frame_slot >= pending_wait_semaphores.size()) {
        return;
    }

    auto& device = context.device;
    if (device && pending_wait_semaphores[frame_slot]) {
        device.destroySemaphore(pending_wait_semaphores[frame_slot]);
    }
    pending_wait_semaphores[frame_slot] = nullptr;
}

void ExternalFrameImporter::destroy(VulkanContext& context) {
    auto& device = context.device;

    for (uint32_t frame_slot = 0; frame_slot < pending_wait_semaphores.size(); ++frame_slot) {
        retire_wait_semaphore(context, frame_slot);
    }

    destroy_imported_image(device, *this);
    this->clear_current_source();
}

void ExternalFrameImporter::clear_current_source() {
    import_extent = vk::Extent2D{};
    source_format = vk::Format::eUndefined;
}

auto ExternalFrameImporter::current_source() const -> ImportedSource {
    return ImportedSource{
        .image = imported_image.image,
        .view = imported_image.view,
        .extent = import_extent,
        .format = source_format,
    };
}

auto ExternalFrameImporter::wait_semaphore(uint32_t frame_slot) const -> vk::Semaphore {
    if (frame_slot >= pending_wait_semaphores.size()) {
        return nullptr;
    }
    return pending_wait_semaphores[frame_slot];
}

} // namespace goggles::render::backend_internal
