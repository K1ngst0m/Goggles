#pragma once

#include <filesystem>
#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

class ShaderRuntime;

struct PassContext {
    uint32_t frame_index;
    vk::Extent2D output_extent;
    vk::Framebuffer target_framebuffer;
    vk::ImageView source_texture;
    vk::ImageView original_texture;
};

class Pass {
public:
    virtual ~Pass() = default;

    Pass() = default;
    Pass(const Pass&) = delete;
    Pass& operator=(const Pass&) = delete;
    Pass(Pass&&) = delete;
    Pass& operator=(Pass&&) = delete;

    [[nodiscard]] virtual auto init(vk::Device device, vk::RenderPass render_pass,
                                    uint32_t num_sync_indices, ShaderRuntime& shader_runtime,
                                    const std::filesystem::path& shader_dir) -> Result<void> = 0;
    virtual void shutdown() = 0;
    virtual void record(vk::CommandBuffer cmd, const PassContext& ctx) = 0;
};

} // namespace goggles::render
