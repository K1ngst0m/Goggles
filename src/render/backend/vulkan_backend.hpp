#pragma once

#include "vulkan_debug.hpp"

#include <SDL3/SDL.h>
#include <array>
#include <capture/capture_receiver.hpp>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <render/chain/filter_chain.hpp>
#include <render/shader/shader_runtime.hpp>
#include <util/error.hpp>
#include <vector>

namespace goggles::render {

class VulkanBackend {
public:
    VulkanBackend() = default;
    ~VulkanBackend();

    VulkanBackend(const VulkanBackend&) = delete;
    VulkanBackend& operator=(const VulkanBackend&) = delete;
    VulkanBackend(VulkanBackend&&) = delete;
    VulkanBackend& operator=(VulkanBackend&&) = delete;

    [[nodiscard]] auto init(SDL_Window* window, bool enable_validation = false,
                            const std::filesystem::path& shader_dir = "shaders") -> Result<void>;
    void shutdown();

    [[nodiscard]] auto render_frame(const CaptureFrame& frame) -> Result<bool>;
    [[nodiscard]] auto render_clear() -> Result<bool>;
    [[nodiscard]] auto handle_resize() -> Result<void>;
    [[nodiscard]] auto is_initialized() const -> bool { return m_initialized; }

    void load_shader_preset(const std::filesystem::path& preset_path);

private:
    [[nodiscard]] auto create_instance(bool enable_validation) -> Result<void>;
    [[nodiscard]] auto create_debug_messenger() -> Result<void>;
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
    [[nodiscard]] auto init_filter_chain() -> Result<void>;

    [[nodiscard]] auto import_dmabuf(const CaptureFrame& frame) -> Result<void>;
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
    std::optional<VulkanDebugMessenger> m_debug_messenger;
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

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    struct FrameResources {
        vk::CommandBuffer command_buffer;
        vk::Fence in_flight_fence;
        vk::Semaphore image_available_sem;
    };
    std::array<FrameResources, MAX_FRAMES_IN_FLIGHT> m_frames{};
    std::vector<vk::Semaphore> m_render_finished_sems;
    uint32_t m_current_frame = 0;

    struct ImportedImage {
        vk::Image image;
        vk::DeviceMemory memory;
        vk::ImageView view;
    };
    ImportedImage m_import;

    ShaderRuntime m_shader_runtime;
    FilterChain m_filter_chain;
    std::filesystem::path m_shader_dir;
    std::filesystem::path m_preset_path;
    vk::Format m_source_format = vk::Format::eUndefined;
    vk::Extent2D m_import_extent{};

    SDL_Window* m_window = nullptr;
    bool m_enable_validation = false;
    bool m_initialized = false;
    bool m_needs_resize = false;
};

} // namespace goggles::render
