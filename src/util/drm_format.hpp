#pragma once

#include <cstdint>
#include <util/drm_fourcc.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::util {

[[nodiscard]] inline vk::Format drm_to_vk_format(uint32_t drm_format) {
    switch (drm_format) {
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
        return vk::Format::eB8G8R8A8Unorm;
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XBGR8888:
        return vk::Format::eR8G8B8A8Unorm;
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_XRGB2101010:
        return vk::Format::eA2R10G10B10UnormPack32;
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_XBGR2101010:
        return vk::Format::eA2B10G10R10UnormPack32;
    case DRM_FORMAT_RGB565:
        return vk::Format::eR5G6B5UnormPack16;
    default:
        return vk::Format::eUndefined;
    }
}

} // namespace goggles::util
