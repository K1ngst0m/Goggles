#pragma once

#include <cstdint>
#include <util/unique_fd.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::util {

enum class ExternalHandleType : std::uint8_t { dmabuf };

struct ExternalImage {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t offset = 0;
    vk::Format format = vk::Format::eUndefined;
    uint64_t modifier = 0;
    util::UniqueFd handle;
    ExternalHandleType handle_type = ExternalHandleType::dmabuf;
};

struct ExternalImageFrame {
    ExternalImage image;
    uint64_t frame_number = 0;
};

} // namespace goggles::util
