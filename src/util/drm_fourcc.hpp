#pragma once

#include <cstdint>

namespace goggles::util {

constexpr auto fourcc_code(char a, char b, char c, char d) -> uint32_t {
    return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) |
           (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24);
}

constexpr uint32_t DRM_FORMAT_RGB565 = fourcc_code('R', 'G', '1', '6');
constexpr uint32_t DRM_FORMAT_XRGB8888 = fourcc_code('X', 'R', '2', '4');
constexpr uint32_t DRM_FORMAT_ARGB8888 = fourcc_code('A', 'R', '2', '4');
constexpr uint32_t DRM_FORMAT_XBGR8888 = fourcc_code('X', 'B', '2', '4');
constexpr uint32_t DRM_FORMAT_ABGR8888 = fourcc_code('A', 'B', '2', '4');
constexpr uint32_t DRM_FORMAT_XRGB2101010 = fourcc_code('X', 'R', '3', '0');
constexpr uint32_t DRM_FORMAT_ARGB2101010 = fourcc_code('A', 'R', '3', '0');
constexpr uint32_t DRM_FORMAT_XBGR2101010 = fourcc_code('X', 'B', '3', '0');
constexpr uint32_t DRM_FORMAT_ABGR2101010 = fourcc_code('A', 'B', '3', '0');

constexpr uint64_t DRM_FORMAT_MOD_LINEAR = 0;
constexpr uint64_t DRM_FORMAT_MOD_INVALID = 0xffffffffffffffULL;

} // namespace goggles::util
