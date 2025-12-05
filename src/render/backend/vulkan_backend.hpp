#pragma once

#include "vulkan_config.hpp"

#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <render/chain/output_pass.hpp>
#include <render/shader/shader_runtime.hpp>
#include <util/error.hpp>
#include <vector>

namespace goggles::render {

struct FrameInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    vk::Format format = vk::Format::eUndefined;
    int dmabuf_fd = -1;
};

class VulkanBackend {
public:
    VulkanBackend() = default;
    ~VulkanBackend();

    VulkanBackend(const VulkanBackend&) = delete;
    VulkanBackend& operator=(const VulkanBackend&) = delete;
    VulkanBackend(VulkanBackend&&) = delete;
    VulkanBackend& operator=(VulkanBackend&&) = delete;

    [[nodiscard]] auto init(SDL_Window* window, const std::filesystem::path& shader_dir = "shaders")
        -> Result<void>;
    void shutdown();

    [[nodiscard]] auto render_frame(const FrameInfo& frame) -> Result<bool>;
    [[nodiscard]] auto render_clear() -> Result<bool>;
    [[nodiscard]] auto handle_resize() -> Result<void>;
    [[nodiscard]] auto is_initialized() const -> bool { return m_initialized; }

private:
    [[nodiscard]] auto create_instance() -> Result<void>;
    [[nodiscard]] auto create_surface(SDL_Window* window) -> Result<void>;
    [[nodiscard]] auto select_physical_device() -> Result<void>;
    [[nodiscard]] auto create_device() -> Result<void>;
    [[nodiscard]] auto create_swapchain(uint32_t width, uint32_t height,
                                        vk::Format preferred_format) -> Result<void>;
    [[nodiscard]] auto recreate_swapchain() -> Result<void>;
    [[nodiscard]] auto recreate_swapchain_for_format(vk::Format source_format) -> Result<void>;
    void cleanup_swapchain();
    [[nodiscard]] auto create_command_resources() -> Result<void>;
    [[nodiscard]] auto create_sync_objects() -> Result<void>;
    [[nodiscard]] auto create_render_pass() -> Result<void>;
    [[nodiscard]] auto create_framebuffers() -> Result<void>;
    [[nodiscard]] auto init_output_pass() -> Result<void>;

    [[nodiscard]] auto import_dmabuf(const FrameInfo& frame) -> Result<void>;
    void cleanup_imported_image();

    [[nodiscard]] auto acquire_next_image() -> Result<uint32_t>;
    [[nodiscard]] auto record_render_commands(vk::CommandBuffer cmd, uint32_t image_index)
        -> Result<void>;
    [[nodiscard]] auto record_clear_commands(vk::CommandBuffer cmd, uint32_t image_index)
        -> Result<void>;
    [[nodiscard]] auto submit_and_present(uint32_t image_index) -> Result<bool>;

    [[nodiscard]] static auto get_matching_swapchain_format(vk::Format source_format) -> vk::Format;
    [[nodiscard]] static auto is_srgb_format(vk::Format format) -> bool;

    vk::UniqueInstance m_instance;
    vk::UniqueSurfaceKHR m_surface;
    vk::UniqueDevice m_device;
    vk::UniqueSwapchainKHR m_swapchain;
    vk::UniqueCommandPool m_command_pool;

    vk::PhysicalDevice m_physical_device;
    uint32_t m_graphics_queue_family = UINT32_MAX;
    vk::Queue m_graphics_queue;

    std::vector<vk::Image> m_swapchain_images;
    std::vector<vk::UniqueImageView> m_swapchain_image_views;
    vk::Format m_swapchain_format = vk::Format::eUndefined;
    vk::Extent2D m_swapchain_extent;

    vk::UniqueRenderPass m_render_pass;
    std::vector<vk::UniqueFramebuffer> m_framebuffers;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    struct FrameResources {
        vk::CommandBuffer command_buffer;
        vk::Fence in_flight_fence;
        vk::Semaphore image_available_sem;
        vk::Semaphore render_finished_sem;
    };
    std::array<FrameResources, MAX_FRAMES_IN_FLIGHT> m_frames{};
    uint32_t m_current_frame = 0;

    vk::Image m_imported_image;
    vk::DeviceMemory m_imported_memory;
    vk::ImageView m_imported_image_view;
    FrameInfo m_current_import{};
    int m_current_import_fd = -1;

    ShaderRuntime m_shader_runtime;
    OutputPass m_output_pass;
    std::filesystem::path m_shader_dir;
    vk::Format m_source_format = vk::Format::eUndefined;

    SDL_Window* m_window = nullptr;
    bool m_initialized = false;
    bool m_needs_resize = false;
};

} // namespace goggles::render
