#include "wsi_virtual.hpp"

#include "ipc_socket.hpp"

#include <bit>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <utility>

#define LAYER_DEBUG(fmt, ...) fprintf(stderr, "[goggles-layer] " fmt "\n", ##__VA_ARGS__)
#define LAYER_WARN(fmt, ...) fprintf(stderr, "[goggles-layer] WARN: " fmt "\n", ##__VA_ARGS__)

namespace goggles::capture {

template <typename Handle>
static auto handle_to_u64(Handle handle) -> uint64_t {
    if constexpr (std::is_pointer_v<Handle>) {
        return static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
    } else {
        return static_cast<uint64_t>(handle);
    }
}

static std::optional<uint32_t> parse_env_uint(const char* name, uint32_t min_val,
                                              uint32_t max_val) {
    const char* env = std::getenv(name);
    if (!env || env[0] == '\0') {
        return std::nullopt;
    }

    char* endptr = nullptr;
    long val = std::strtol(env, &endptr, 10);

    if (endptr == env || *endptr != '\0') {
        LAYER_WARN("%s='%s' is not a valid integer, ignoring", name, env);
        return std::nullopt;
    }

    if (val < 0 || std::cmp_less(val, min_val) || std::cmp_greater(val, max_val)) {
        LAYER_WARN("%s=%ld is out of range [%u, %u], ignoring", name, val, min_val, max_val);
        return std::nullopt;
    }

    return static_cast<uint32_t>(val);
}

static uint32_t get_fps_limit() {
    static const uint32_t limit = parse_env_uint("GOGGLES_FPS_LIMIT", 0, 1000).value_or(60);
    return limit;
}

bool should_use_wsi_proxy() {
    static const bool enabled = []() {
        const char* proxy = std::getenv("GOGGLES_WSI_PROXY");
        const char* capture = std::getenv("GOGGLES_CAPTURE");
        return proxy && std::strcmp(proxy, "0") != 0 && capture && std::strcmp(capture, "0") != 0;
    }();
    return enabled;
}

WsiVirtualizer& WsiVirtualizer::instance() {
    static WsiVirtualizer inst;
    return inst;
}

WsiVirtualizer::WsiVirtualizer() : enabled_(should_use_wsi_proxy()) {
    if (enabled_) {
        LAYER_DEBUG("WSI proxy mode enabled");
    }
}

WsiVirtualizer::~WsiVirtualizer() {
    for (auto& [handle, swap] : swapchains_) {
        for (int fd : swap.dmabuf_fds) {
            if (fd >= 0) {
                close(fd);
            }
        }
    }
}

VkSurfaceKHR WsiVirtualizer::generate_surface_handle() {
    return std::bit_cast<VkSurfaceKHR>(next_handle_++);
}

VkSwapchainKHR WsiVirtualizer::generate_swapchain_handle() {
    return std::bit_cast<VkSwapchainKHR>(next_handle_++);
}

VkResult WsiVirtualizer::create_surface(VkInstance inst, VkSurfaceKHR* surface) {
    std::lock_guard lock(mutex_);

    VirtualSurface vs{};
    vs.handle = generate_surface_handle();
    vs.instance = inst;
    vs.width = parse_env_uint("GOGGLES_WIDTH", 1, 16384).value_or(1920);
    vs.height = parse_env_uint("GOGGLES_HEIGHT", 1, 16384).value_or(1080);

    surfaces_[vs.handle] = vs;
    *surface = vs.handle;

    LAYER_DEBUG("Virtual surface created: 0x%016" PRIx64 " (%ux%u)", handle_to_u64(vs.handle),
                vs.width, vs.height);
    return VK_SUCCESS;
}

void WsiVirtualizer::destroy_surface(VkInstance /*instance*/, VkSurfaceKHR surface) {
    std::lock_guard lock(mutex_);
    surfaces_.erase(surface);
}

bool WsiVirtualizer::is_virtual_surface(VkSurfaceKHR surface) {
    std::lock_guard lock(mutex_);
    return surfaces_.find(surface) != surfaces_.end();
}

VirtualSurface* WsiVirtualizer::get_surface(VkSurfaceKHR surface) {
    std::lock_guard lock(mutex_);
    auto it = surfaces_.find(surface);
    return it != surfaces_.end() ? &it->second : nullptr;
}

void WsiVirtualizer::set_resolution(uint32_t width, uint32_t height) {
    std::lock_guard lock(mutex_);
    for (auto& [handle, surface] : surfaces_) {
        if (surface.width != width || surface.height != height) {
            surface.width = width;
            surface.height = height;
            surface.out_of_date = true;
            LAYER_DEBUG("Virtual surface 0x%016" PRIx64 " resolution changed to %ux%u",
                        handle_to_u64(handle), width, height);
        }
    }
}

VkResult WsiVirtualizer::get_surface_capabilities(VkPhysicalDevice /*physDev*/,
                                                  VkSurfaceKHR surface,
                                                  VkSurfaceCapabilitiesKHR* caps) {
    std::lock_guard lock(mutex_);
    auto it = surfaces_.find(surface);
    if (it == surfaces_.end()) {
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    auto& vs = it->second;
    caps->minImageCount = 2;
    caps->maxImageCount = 3;
    caps->currentExtent = {vs.width, vs.height};
    caps->minImageExtent = {vs.width, vs.height};
    caps->maxImageExtent = {vs.width, vs.height};
    caps->maxImageArrayLayers = 1;
    caps->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    caps->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    caps->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    caps->supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return VK_SUCCESS;
}

VkResult WsiVirtualizer::get_surface_formats(VkPhysicalDevice /*physDev*/, VkSurfaceKHR /*surface*/,
                                             uint32_t* count, VkSurfaceFormatKHR* formats) {
    constexpr VkSurfaceFormatKHR supported[] = {
        {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
    };

    if (!formats) {
        *count = 2;
        return VK_SUCCESS;
    }

    uint32_t to_copy = *count < 2 ? *count : 2;
    std::memcpy(formats, supported, to_copy * sizeof(VkSurfaceFormatKHR));
    *count = to_copy;
    return to_copy < 2 ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult WsiVirtualizer::get_surface_present_modes(VkPhysicalDevice /*physDev*/,
                                                   VkSurfaceKHR /*surface*/, uint32_t* count,
                                                   VkPresentModeKHR* modes) {
    constexpr VkPresentModeKHR supported[] = {
        VK_PRESENT_MODE_FIFO_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR,
    };

    if (!modes) {
        *count = 2;
        return VK_SUCCESS;
    }

    uint32_t to_copy = *count < 2 ? *count : 2;
    std::memcpy(modes, supported, to_copy * sizeof(VkPresentModeKHR));
    *count = to_copy;
    return to_copy < 2 ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult WsiVirtualizer::get_surface_support(VkPhysicalDevice phys_dev, uint32_t queue_family,
                                             VkSurfaceKHR /*surface*/, VkBool32* supported,
                                             VkInstData* inst_data) {
    uint32_t count = 0;
    inst_data->funcs.GetPhysicalDeviceQueueFamilyProperties(phys_dev, &count, nullptr);
    if (queue_family >= count) {
        *supported = VK_FALSE;
        return VK_SUCCESS;
    }

    std::vector<VkQueueFamilyProperties> props(count);
    inst_data->funcs.GetPhysicalDeviceQueueFamilyProperties(phys_dev, &count, props.data());

    *supported = (props[queue_family].queueFlags & VK_QUEUE_GRAPHICS_BIT) ? VK_TRUE : VK_FALSE;
    return VK_SUCCESS;
}

static uint32_t find_memory_type(const VkPhysicalDeviceMemoryProperties& props,
                                 uint32_t type_bits) {
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((type_bits & (1u << i)) &&
            (props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            return i;
        }
    }
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if (type_bits & (1u << i)) {
            return i;
        }
    }
    return UINT32_MAX;
}

// from drm_fourcc.h (kept local to avoid adding extra headers in the layer build)
constexpr uint64_t DRM_FORMAT_MOD_LINEAR = 0;
constexpr uint64_t DRM_FORMAT_MOD_INVALID = 0xffffffffffffffULL;

struct ModifierInfo {
    uint64_t modifier;
    VkFormatFeatureFlags features;
    uint32_t plane_count;
};

static std::vector<ModifierInfo> query_export_modifiers(VkPhysicalDevice phys_device,
                                                        VkFormat format, VkInstFuncs* inst_funcs,
                                                        VkFormatFeatureFlags required_features) {
    std::vector<ModifierInfo> result;

    if (!inst_funcs->GetPhysicalDeviceFormatProperties2) {
        return result;
    }

    VkDrmFormatModifierPropertiesListEXT modifier_list{};
    modifier_list.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;

    VkFormatProperties2 format_props{};
    format_props.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    format_props.pNext = &modifier_list;

    inst_funcs->GetPhysicalDeviceFormatProperties2(phys_device, format, &format_props);

    if (modifier_list.drmFormatModifierCount == 0) {
        return result;
    }

    std::vector<VkDrmFormatModifierPropertiesEXT> modifiers(modifier_list.drmFormatModifierCount);
    modifier_list.pDrmFormatModifierProperties = modifiers.data();

    inst_funcs->GetPhysicalDeviceFormatProperties2(phys_device, format, &format_props);

    for (const auto& mod : modifiers) {
        if (((mod.drmFormatModifierTilingFeatures & required_features) == required_features) &&
            mod.drmFormatModifierPlaneCount == 1) { // Single-plane only for now
            result.push_back({
                mod.drmFormatModifier,
                mod.drmFormatModifierTilingFeatures,
                mod.drmFormatModifierPlaneCount,
            });
        }
    }

    return result;
}

static auto create_swapchain_image(VkDevice device, VkDeviceData* dev_data,
                                   const VirtualSwapchain& swap, bool use_modifier_tiling,
                                   const std::vector<uint64_t>& modifier_list, VkImage* out_image)
    -> VkResult {
    auto& funcs = dev_data->funcs;

    VkExternalMemoryImageCreateInfo ext_mem_info{};
    ext_mem_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    ext_mem_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkImageDrmFormatModifierListCreateInfoEXT modifier_list_info{};
    if (use_modifier_tiling) {
        modifier_list_info.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
        modifier_list_info.drmFormatModifierCount = static_cast<uint32_t>(modifier_list.size());
        modifier_list_info.pDrmFormatModifiers = modifier_list.data();
        ext_mem_info.pNext = &modifier_list_info;
    }

    VkImageCreateInfo img_info{};
    img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img_info.pNext = &ext_mem_info;
    img_info.imageType = VK_IMAGE_TYPE_2D;
    img_info.format = swap.format;
    img_info.extent = {swap.extent.width, swap.extent.height, 1};
    img_info.mipLevels = 1;
    img_info.arrayLayers = 1;
    img_info.samples = VK_SAMPLE_COUNT_1_BIT;
    img_info.tiling =
        use_modifier_tiling ? VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT : VK_IMAGE_TILING_LINEAR;
    img_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    return funcs.CreateImage(device, &img_info, nullptr, out_image);
}

static auto get_image_modifier(VkDevice device, VkDeviceData* dev_data, VkImage image) -> uint64_t {
    auto& funcs = dev_data->funcs;

    if (!funcs.GetImageDrmFormatModifierPropertiesEXT) {
        return DRM_FORMAT_MOD_INVALID;
    }

    VkImageDrmFormatModifierPropertiesEXT modifier_props{};
    modifier_props.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT;
    VkResult mod_res = funcs.GetImageDrmFormatModifierPropertiesEXT(device, image, &modifier_props);
    if (mod_res != VK_SUCCESS) {
        LAYER_DEBUG("Virtual swapchain: failed to query DRM modifier (%d)", mod_res);
        return DRM_FORMAT_MOD_INVALID;
    }
    return modifier_props.drmFormatModifier;
}

struct ImageLayout {
    uint32_t stride;
    uint32_t offset;
};

static auto get_image_layout(VkDevice device, VkDeviceData* dev_data, VkImage image,
                             uint32_t image_index) -> std::optional<ImageLayout> {
    auto& funcs = dev_data->funcs;

    VkImageSubresource subres{};
    subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subres.mipLevel = 0;
    subres.arrayLayer = 0;
    VkSubresourceLayout layout{};
    funcs.GetImageSubresourceLayout(device, image, &subres, &layout);

    if (layout.rowPitch > UINT32_MAX) {
        LAYER_DEBUG("Virtual swapchain image %u: stride %" PRIu64 " exceeds uint32_t max",
                    image_index, layout.rowPitch);
        return std::nullopt;
    }
    if (layout.offset > UINT32_MAX) {
        LAYER_DEBUG("Virtual swapchain image %u: offset %" PRIu64 " exceeds uint32_t max",
                    image_index, layout.offset);
        return std::nullopt;
    }

    return ImageLayout{
        .stride = static_cast<uint32_t>(layout.rowPitch),
        .offset = static_cast<uint32_t>(layout.offset),
    };
}

static auto export_dmabuf_fd(VkDevice device, VkDeviceData* dev_data, VkDeviceMemory memory,
                             uint32_t image_index, int* out_fd) -> bool {
    auto& funcs = dev_data->funcs;

    VkMemoryGetFdInfoKHR fd_info{};
    fd_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    fd_info.memory = memory;
    fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    int fd = -1;
    if (funcs.GetMemoryFdKHR(device, &fd_info, &fd) != VK_SUCCESS || fd < 0) {
        LAYER_DEBUG("Failed to export DMA-BUF for virtual swapchain image %u", image_index);
        return false;
    }

    *out_fd = fd;
    return true;
}

static auto alloc_exportable_memory(VkDevice device, VkDeviceData* dev_data,
                                    const VkPhysicalDeviceMemoryProperties& mem_props,
                                    VkImage image, uint32_t image_index, VkDeviceMemory* out_memory)
    -> bool {
    auto& funcs = dev_data->funcs;

    VkMemoryRequirements mem_reqs;
    funcs.GetImageMemoryRequirements(device, image, &mem_reqs);

    uint32_t mem_type = find_memory_type(mem_props, mem_reqs.memoryTypeBits);
    if (mem_type == UINT32_MAX) {
        LAYER_DEBUG("No suitable memory type for virtual swapchain");
        return false;
    }

    VkExportMemoryAllocateInfo export_info{};
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = &export_info;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (funcs.AllocateMemory(device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        LAYER_DEBUG("Failed to allocate memory for virtual swapchain image %u", image_index);
        return false;
    }

    if (funcs.BindImageMemory(device, image, memory, 0) != VK_SUCCESS) {
        funcs.FreeMemory(device, memory, nullptr);
        LAYER_DEBUG("Failed to bind memory for virtual swapchain image %u", image_index);
        return false;
    }

    *out_memory = memory;
    return true;
}

static auto create_exportable_image(VirtualSwapchain& swap, VkDevice device, VkDeviceData* dev_data,
                                    const VkPhysicalDeviceMemoryProperties& mem_props,
                                    bool use_modifier_tiling,
                                    const std::vector<uint64_t>& modifier_list,
                                    uint32_t image_index) -> bool {
    auto& funcs = dev_data->funcs;

    VkImage image = VK_NULL_HANDLE;
    VkResult create_res =
        create_swapchain_image(device, dev_data, swap, use_modifier_tiling, modifier_list, &image);
    if (create_res != VK_SUCCESS) {
        LAYER_DEBUG("Failed to create virtual swapchain image %u (%d)", image_index, create_res);
        return false;
    }

    uint64_t modifier = DRM_FORMAT_MOD_LINEAR;
    if (use_modifier_tiling) {
        modifier = get_image_modifier(device, dev_data, image);
        if (modifier == DRM_FORMAT_MOD_INVALID) {
            // We created an image using a modifier list, but couldn't query what was chosen.
            // Falling back to LINEAR tiling avoids exporting an invalid modifier.
            LAYER_DEBUG("Virtual swapchain: falling back to LINEAR tiling due to invalid modifier "
                        "(0x%" PRIx64 ")",
                        modifier);
            funcs.DestroyImage(device, image, nullptr);
            image = VK_NULL_HANDLE;

            create_res =
                create_swapchain_image(device, dev_data, swap, false, modifier_list, &image);
            if (create_res != VK_SUCCESS) {
                LAYER_DEBUG("Failed to create LINEAR fallback image %u (%d)", image_index,
                            create_res);
                return false;
            }
            modifier = DRM_FORMAT_MOD_LINEAR;
        }
    }

    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!alloc_exportable_memory(device, dev_data, mem_props, image, image_index, &memory)) {
        funcs.DestroyImage(device, image, nullptr);
        return false;
    }

    auto layout = get_image_layout(device, dev_data, image, image_index);
    if (!layout) {
        funcs.FreeMemory(device, memory, nullptr);
        funcs.DestroyImage(device, image, nullptr);
        return false;
    }

    int fd = -1;
    if (!export_dmabuf_fd(device, dev_data, memory, image_index, &fd)) {
        funcs.FreeMemory(device, memory, nullptr);
        funcs.DestroyImage(device, image, nullptr);
        return false;
    }

    swap.images.push_back(image);
    swap.modifiers.push_back(modifier);
    swap.memory.push_back(memory);
    swap.strides.push_back(layout->stride);
    swap.offsets.push_back(layout->offset);
    swap.dmabuf_fds.push_back(fd);

    LAYER_DEBUG("Virtual swapchain image %u: fd=%d, stride=%u, offset=%u, modifier=0x%" PRIx64,
                image_index, fd, layout->stride, layout->offset, modifier);
    return true;
}

bool WsiVirtualizer::create_exportable_images(VirtualSwapchain& swap, VkDevice device,
                                              VkDeviceData* dev_data) {
    auto& funcs = dev_data->funcs;
    auto* inst_data = dev_data->inst_data;

    VkPhysicalDeviceMemoryProperties mem_props;
    inst_data->funcs.GetPhysicalDeviceMemoryProperties(dev_data->physical_device, &mem_props);

    // Prefer DRM modifier tiling to keep the format stable (e.g. SRGB) and to export a correct
    // modifier to the viewer. Fall back to LINEAR only if no suitable modifier is available.
    constexpr VkFormatFeatureFlags required_features = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                                       VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                                       VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    auto modifiers = query_export_modifiers(dev_data->physical_device, swap.format,
                                            &inst_data->funcs, required_features);

    bool use_modifier_tiling = !modifiers.empty();
    if (use_modifier_tiling && !funcs.GetImageDrmFormatModifierPropertiesEXT) {
        // We can create images using a modifier list, but without being able to query the chosen
        // modifier we cannot safely export/import the DMA-BUF.
        LAYER_DEBUG("Virtual swapchain: GetImageDrmFormatModifierPropertiesEXT unavailable; "
                    "falling back to LINEAR tiling");
        use_modifier_tiling = false;
    }
    std::vector<uint64_t> modifier_list;
    if (use_modifier_tiling) {
        modifier_list.reserve(modifiers.size());
        for (const auto& mod : modifiers) {
            modifier_list.push_back(mod.modifier);
        }
        LAYER_DEBUG("Virtual swapchain: using DRM modifier list with %zu modifiers for format %d",
                    modifier_list.size(), swap.format);
    } else {
        LAYER_DEBUG("Virtual swapchain: no suitable DRM modifiers found, falling back to LINEAR");
    }

    for (uint32_t i = 0; i < swap.image_count; ++i) {
        if (!create_exportable_image(swap, device, dev_data, mem_props, use_modifier_tiling,
                                     modifier_list, i)) {
            return false;
        }
    }

    return true;
}

VkResult WsiVirtualizer::create_swapchain(VkDevice device, const VkSwapchainCreateInfoKHR* info,
                                          VkSwapchainKHR* swapchain, VkDeviceData* dev_data) {
    std::lock_guard lock(mutex_);

    VirtualSwapchain swap{};
    swap.handle = generate_swapchain_handle();
    swap.device = device;
    swap.surface = info->surface;
    swap.format = info->imageFormat;
    swap.extent = info->imageExtent;
    swap.image_count = info->minImageCount < 2 ? 2 : info->minImageCount;
    if (swap.image_count > 3) {
        swap.image_count = 3;
    }

    if (!create_exportable_images(swap, device, dev_data)) {
        destroy_swapchain_resources(swap, device, dev_data);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkSwapchainKHR handle = swap.handle;
    swapchains_[handle] = std::move(swap);
    *swapchain = handle;

    LAYER_DEBUG("Virtual swapchain created: 0x%016" PRIx64 " (%ux%u, %u images)",
                handle_to_u64(handle), swapchains_[handle].extent.width,
                swapchains_[handle].extent.height, swapchains_[handle].image_count);
    return VK_SUCCESS;
}

void WsiVirtualizer::destroy_swapchain_resources(VirtualSwapchain& swap, VkDevice device,
                                                 VkDeviceData* dev_data) {
    auto& funcs = dev_data->funcs;

    for (int fd : swap.dmabuf_fds) {
        if (fd >= 0) {
            close(fd);
        }
    }
    swap.dmabuf_fds.clear();

    for (auto mem : swap.memory) {
        if (mem != VK_NULL_HANDLE) {
            funcs.FreeMemory(device, mem, nullptr);
        }
    }
    swap.memory.clear();

    for (auto img : swap.images) {
        if (img != VK_NULL_HANDLE) {
            funcs.DestroyImage(device, img, nullptr);
        }
    }
    swap.images.clear();
}

void WsiVirtualizer::destroy_swapchain(VkDevice device, VkSwapchainKHR swapchain,
                                       VkDeviceData* dev_data) {
    std::lock_guard lock(mutex_);
    auto it = swapchains_.find(swapchain);
    if (it == swapchains_.end()) {
        return;
    }

    destroy_swapchain_resources(it->second, device, dev_data);
    swapchains_.erase(it);
}

bool WsiVirtualizer::is_virtual_swapchain(VkSwapchainKHR swapchain) {
    std::lock_guard lock(mutex_);
    return swapchains_.find(swapchain) != swapchains_.end();
}

VirtualSwapchain* WsiVirtualizer::get_swapchain(VkSwapchainKHR swapchain) {
    std::lock_guard lock(mutex_);
    auto it = swapchains_.find(swapchain);
    return it != swapchains_.end() ? &it->second : nullptr;
}

VkResult WsiVirtualizer::get_swapchain_images(VkSwapchainKHR swapchain, uint32_t* count,
                                              VkImage* images) {
    std::lock_guard lock(mutex_);
    auto it = swapchains_.find(swapchain);
    if (it == swapchains_.end()) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    auto& swap = it->second;
    if (!images) {
        *count = swap.image_count;
        return VK_SUCCESS;
    }

    uint32_t to_copy = *count < swap.image_count ? *count : swap.image_count;
    for (uint32_t i = 0; i < to_copy; ++i) {
        images[i] = swap.images[i];
    }
    *count = to_copy;
    return to_copy < swap.image_count ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult WsiVirtualizer::acquire_next_image(VkDevice /*device*/, VkSwapchainKHR swapchain,
                                            uint64_t /*timeout*/, VkSemaphore semaphore,
                                            VkFence fence, uint32_t* index,
                                            VkDeviceData* dev_data) {
    // Poll for resolution requests before acquiring
    auto& socket = get_layer_socket();
    CaptureControl ctrl{};
    socket.poll_control(ctrl);
    auto res_req = socket.consume_resolution_request();
    if (res_req.pending) {
        set_resolution(res_req.width, res_req.height);
    }

    uint32_t fps = get_fps_limit();
    if (fps > 0) {
        std::chrono::steady_clock::time_point last_acquire;
        {
            std::lock_guard lock(mutex_);
            auto it = swapchains_.find(swapchain);
            if (it == swapchains_.end()) {
                return VK_ERROR_OUT_OF_DATE_KHR;
            }
            last_acquire = it->second.last_acquire;
        }

        using namespace std::chrono;
        auto frame_duration = nanoseconds(1'000'000'000 / fps);
        auto next_frame = last_acquire + frame_duration;
        if (steady_clock::now() < next_frame) {
            std::this_thread::sleep_until(next_frame);
        }
    }

    uint32_t current_idx;
    {
        std::lock_guard lock(mutex_);
        auto it = swapchains_.find(swapchain);
        if (it == swapchains_.end()) {
            return VK_ERROR_OUT_OF_DATE_KHR;
        }

        auto& swap = it->second;

        auto surf_it = surfaces_.find(swap.surface);
        if (surf_it != surfaces_.end() && surf_it->second.out_of_date) {
            surf_it->second.out_of_date = false;
            LAYER_DEBUG("Swapchain out-of-date due to resolution change: %ux%u",
                        surf_it->second.width, surf_it->second.height);
            return VK_ERROR_OUT_OF_DATE_KHR;
        }

        current_idx = swap.current_index;
        swap.current_index = (current_idx + 1) % swap.image_count;
        swap.last_acquire = std::chrono::steady_clock::now();
    }

    *index = current_idx;

    auto& funcs = dev_data->funcs;

    if (semaphore != VK_NULL_HANDLE || fence != VK_NULL_HANDLE) {
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        if (semaphore != VK_NULL_HANDLE) {
            submit.signalSemaphoreCount = 1;
            submit.pSignalSemaphores = &semaphore;
        }
        funcs.QueueSubmit(dev_data->graphics_queue, 1, &submit, fence);
    }

    return VK_SUCCESS;
}

SwapchainFrameData WsiVirtualizer::get_frame_data(VkSwapchainKHR swapchain, uint32_t image_index) {
    std::lock_guard lock(mutex_);
    auto it = swapchains_.find(swapchain);
    if (it == swapchains_.end()) {
        return {};
    }

    auto& swap = it->second;
    if (image_index >= swap.dmabuf_fds.size() || image_index >= swap.strides.size() ||
        image_index >= swap.offsets.size() || image_index >= swap.modifiers.size()) {
        return {};
    }

    return {
        .valid = true,
        .width = swap.extent.width,
        .height = swap.extent.height,
        .format = swap.format,
        .stride = swap.strides[image_index],
        .offset = swap.offsets[image_index],
        .modifier = swap.modifiers[image_index],
        .dmabuf_fd = swap.dmabuf_fds[image_index],
    };
}

} // namespace goggles::capture
