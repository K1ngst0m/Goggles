#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <util/config.hpp>
#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

class ShaderRuntime;

struct VulkanContext {
    vk::Device device;
    vk::PhysicalDevice physical_device;
    vk::CommandPool command_pool;
    vk::Queue graphics_queue;
};

struct PassContext {
    uint32_t frame_index;
    vk::Extent2D output_extent;
    vk::Extent2D source_extent;
    vk::ImageView target_image_view;
    vk::Format target_format;
    vk::ImageView source_texture;
    vk::ImageView original_texture;
    ScaleMode scale_mode = ScaleMode::stretch;
    uint32_t integer_scale = 0;
};

class Pass {
public:
    virtual ~Pass() = default;

    Pass() = default;
    Pass(const Pass&) = delete;
    Pass& operator=(const Pass&) = delete;
    Pass(Pass&&) = delete;
    Pass& operator=(Pass&&) = delete;

    virtual void shutdown() = 0;
    virtual void record(vk::CommandBuffer cmd, const PassContext& ctx) = 0;
};

struct ScaledViewport {
    int32_t offset_x = 0;
    int32_t offset_y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

[[nodiscard]] inline auto calculate_viewport(uint32_t source_width, uint32_t source_height,
                                             uint32_t target_width, uint32_t target_height,
                                             ScaleMode mode, uint32_t integer_scale = 0)
    -> ScaledViewport {
    ScaledViewport result;

    if (source_width == 0 || source_height == 0 || target_width == 0 || target_height == 0) {
        return result;
    }

    switch (mode) {
    case ScaleMode::stretch: {
        result.offset_x = 0;
        result.offset_y = 0;
        result.width = target_width;
        result.height = target_height;
        break;
    }

    case ScaleMode::fit: {
        float source_aspect = static_cast<float>(source_width) / static_cast<float>(source_height);
        float target_aspect = static_cast<float>(target_width) / static_cast<float>(target_height);

        if (source_aspect > target_aspect) {
            result.width = target_width;
            result.height =
                static_cast<uint32_t>(std::round(static_cast<float>(target_width) / source_aspect));
        } else {
            result.height = target_height;
            result.width = static_cast<uint32_t>(
                std::round(static_cast<float>(target_height) * source_aspect));
        }

        result.offset_x = static_cast<int32_t>((target_width - result.width) / 2);
        result.offset_y = static_cast<int32_t>((target_height - result.height) / 2);
        break;
    }

    case ScaleMode::fill: {
        float source_aspect = static_cast<float>(source_width) / static_cast<float>(source_height);
        float target_aspect = static_cast<float>(target_width) / static_cast<float>(target_height);

        if (source_aspect > target_aspect) {
            result.height = target_height;
            result.width = static_cast<uint32_t>(
                std::round(static_cast<float>(target_height) * source_aspect));
        } else {
            result.width = target_width;
            result.height =
                static_cast<uint32_t>(std::round(static_cast<float>(target_width) / source_aspect));
        }

        result.offset_x = static_cast<int32_t>(target_width - result.width) / 2;
        result.offset_y = static_cast<int32_t>(target_height - result.height) / 2;
        break;
    }

    case ScaleMode::integer: {
        uint32_t scale = integer_scale;

        if (scale == 0) {
            uint32_t max_scale_x = target_width / source_width;
            uint32_t max_scale_y = target_height / source_height;
            scale = std::max(1U, std::min(max_scale_x, max_scale_y));
        }

        result.width = source_width * scale;
        result.height = source_height * scale;

        result.offset_x = static_cast<int32_t>(target_width - result.width) / 2;
        result.offset_y = static_cast<int32_t>(target_height - result.height) / 2;
        break;
    }
    }

    return result;
}

} // namespace goggles::render
