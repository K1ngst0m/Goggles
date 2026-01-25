#pragma once

#include <cstdint>
#include <util/drm_fourcc.hpp>
#include <vulkan/vulkan.h>

namespace goggles::util {

[[nodiscard]] inline VkFormat drm_to_vk_format(uint32_t drm_format) {
    switch (drm_format) {
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XBGR8888:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_XRGB2101010:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_XBGR2101010:
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case DRM_FORMAT_RGB565:
        return VK_FORMAT_R5G6B5_UNORM_PACK16;
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

} // namespace goggles::util
