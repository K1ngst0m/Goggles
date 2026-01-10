#pragma once

#include "vulkan_debug.hpp"

#include <SDL3/SDL.h>
#include <array>
#include <atomic>
#include <capture/capture_receiver.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <optional>
#include <render/chain/filter_chain.hpp>
#include <render/shader/shader_runtime.hpp>
#include <util/config.hpp>
#include <util/error.hpp>
#include <vector>

namespace goggles::render {

struct RenderSettings {
    ScaleMode scale_mode = ScaleMode::stretch;
    uint32_t integer_scale = 0;
    uint32_t target_fps = 60;
};

class VulkanBackend {
public:
    [[nodiscard]] static auto create(SDL_Window* window, bool enable_validation = false,
                                     const std::filesystem::path& shader_dir = "shaders",
                                     RenderSettings settings = {}) -> ResultPtr<VulkanBackend>;

    ~VulkanBackend();

    VulkanBackend(const VulkanBackend&) = delete;
    VulkanBackend& operator=(const VulkanBackend&) = delete;
    VulkanBackend(VulkanBackend&&) = delete;
    VulkanBackend& operator=(VulkanBackend&&) = delete;

    void shutdown();

    [[nodiscard]] auto render_frame(const CaptureFrame& frame) -> Result<bool>;
    [[nodiscard]] auto render_clear() -> Result<bool>;
    [[nodiscard]] auto handle_resize() -> Result<void>;

    void load_shader_preset(const std::filesystem::path& preset_path);
    [[nodiscard]] auto reload_shader_preset(const std::filesystem::path& preset_path)
        -> Result<void>;
    [[nodiscard]] auto current_preset_path() const -> const std::filesystem::path& {
        return m_preset_path;
    }

    void set_shader_enabled(bool enabled);

    using UiRenderCallback = std::function<void(vk::CommandBuffer, vk::ImageView, vk::Extent2D)>;
    [[nodiscard]] auto render_frame_with_ui(const CaptureFrame& frame,
                                            const UiRenderCallback& ui_callback) -> Result<bool>;
    [[nodiscard]] auto render_clear_with_ui(const UiRenderCallback& ui_callback) -> Result<bool>;

    [[nodiscard]] auto needs_format_rebuild(vk::Format source_format) const -> bool;
    [[nodiscard]] auto rebuild_for_format(vk::Format source_format) -> Result<void>;
    void wait_all_frames();

    [[nodiscard]] auto instance() const -> vk::Instance { return *m_instance; }
    [[nodiscard]] auto physical_device() const -> vk::PhysicalDevice { return m_physical_device; }
    [[nodiscard]] auto device() const -> vk::Device { return *m_device; }
    [[nodiscard]] auto graphics_queue() const -> vk::Queue { return m_graphics_queue; }
    [[nodiscard]] auto graphics_queue_family() const -> uint32_t { return m_graphics_queue_family; }
    [[nodiscard]] auto swapchain_format() const -> vk::Format { return m_swapchain_format; }
    [[nodiscard]] auto swapchain_image_count() const -> uint32_t {
        return static_cast<uint32_t>(m_swapchain_images.size());
    }
    [[nodiscard]] auto filter_chain() -> FilterChain* { return m_filter_chain.get(); }
    [[nodiscard]] auto gpu_index() const -> uint32_t { return m_gpu_index; }
    [[nodiscard]] auto gpu_uuid() const -> const std::string& { return m_gpu_uuid; }

    [[nodiscard]] auto import_sync_semaphores(util::UniqueFd frame_ready_fd,
                                              util::UniqueFd frame_consumed_fd) -> Result<void>;
    [[nodiscard]] auto has_sync_semaphores() const -> bool { return m_sync_semaphores_imported; }
    void cleanup_sync_semaphores();

    [[nodiscard]] auto consume_chain_swapped() -> bool {
        return m_chain_swapped.exchange(false, std::memory_order_acq_rel);
    }

private:
    VulkanBackend() = default;

    void update_target_fps(uint32_t target_fps) {
        m_target_fps = target_fps;
        m_last_present_time = std::chrono::steady_clock::time_point{};
    }

    [[nodiscard]] auto create_instance(bool enable_validation) -> Result<void>;
    [[nodiscard]] auto create_debug_messenger() -> Result<void>;
    [[nodiscard]] auto create_surface(SDL_Window* window) -> Result<void>;
    [[nodiscard]] auto select_physical_device() -> Result<void>;
    [[nodiscard]] auto create_device() -> Result<void>;

    [[nodiscard]] auto is_present_wait_ready() const -> bool { return m_present_wait_supported; }
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
    [[nodiscard]] auto record_render_commands(vk::CommandBuffer cmd, uint32_t image_index,
                                              const UiRenderCallback& ui_callback = nullptr)
        -> Result<void>;
    [[nodiscard]] auto record_clear_commands(vk::CommandBuffer cmd, uint32_t image_index,
                                             const UiRenderCallback& ui_callback = nullptr)
        -> Result<void>;
    [[nodiscard]] auto submit_and_present(uint32_t image_index) -> Result<bool>;

    [[nodiscard]] static auto get_matching_swapchain_format(vk::Format source_format) -> vk::Format;
    [[nodiscard]] static auto is_srgb_format(vk::Format format) -> bool;

    [[nodiscard]] auto apply_present_wait(uint64_t present_id) -> Result<void>;
    void throttle_present();

    vk::PhysicalDevice m_physical_device;
    vk::Queue m_graphics_queue;
    std::unique_ptr<ShaderRuntime> m_shader_runtime;
    std::unique_ptr<FilterChain> m_filter_chain;
    SDL_Window* m_window = nullptr;
    vk::Semaphore m_frame_ready_sem;
    vk::Semaphore m_frame_consumed_sem;
    uint64_t m_last_frame_number = 0;
    uint64_t m_last_signaled_frame = 0;

    vk::UniqueInstance m_instance;
    vk::UniqueDevice m_device;
    std::vector<vk::Image> m_swapchain_images;
    std::vector<vk::UniqueImageView> m_swapchain_image_views;
    std::vector<vk::Semaphore> m_render_finished_sems;

    struct ImportedImage {
        vk::Image image;
        vk::DeviceMemory memory;
        vk::ImageView view;
    };
    ImportedImage m_import;

    vk::UniqueSurfaceKHR m_surface;
    vk::UniqueSwapchainKHR m_swapchain;
    vk::UniqueCommandPool m_command_pool;
    std::optional<VulkanDebugMessenger> m_debug_messenger;

    std::filesystem::path m_shader_dir;
    std::filesystem::path m_preset_path;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    struct FrameResources {
        vk::CommandBuffer command_buffer;
        vk::Fence in_flight_fence;
        vk::Semaphore image_available_sem;
    };
    std::array<FrameResources, MAX_FRAMES_IN_FLIGHT> m_frames{};

    uint32_t m_graphics_queue_family = UINT32_MAX;
    uint32_t m_gpu_index = 0;
    std::string m_gpu_uuid;
    vk::Format m_swapchain_format = vk::Format::eUndefined;
    uint32_t m_current_frame = 0;
    vk::Format m_source_format = vk::Format::eUndefined;
    uint32_t m_integer_scale = 0;
    vk::Extent2D m_swapchain_extent;
    vk::Extent2D m_import_extent;
    bool m_enable_validation = false;
    ScaleMode m_scale_mode = ScaleMode::stretch;
    bool m_needs_resize = false;
    bool m_sync_semaphores_imported = false;
    bool m_present_wait_supported = false;
    uint32_t m_target_fps = 0;
    uint64_t m_present_id = 0;
    std::chrono::steady_clock::time_point m_last_present_time;
    std::atomic<bool> m_format_changed{false};
    std::atomic<bool> m_chain_swapped{false};

    // Async shader reload state
    std::unique_ptr<FilterChain> m_pending_filter_chain;
    std::unique_ptr<ShaderRuntime> m_pending_shader_runtime;
    std::filesystem::path m_pending_preset_path;
    std::atomic<bool> m_pending_chain_ready{false};
    std::future<Result<void>> m_pending_load_future;

    struct DeferredDestroy {
        std::unique_ptr<FilterChain> chain;
        std::unique_ptr<ShaderRuntime> runtime;
        uint64_t destroy_after_frame = 0;
    };
    static constexpr size_t MAX_DEFERRED_DESTROYS = 4;
    std::array<DeferredDestroy, MAX_DEFERRED_DESTROYS> m_deferred_destroys{};
    size_t m_deferred_count = 0;
    uint64_t m_frame_count = 0;

    void check_pending_chain_swap();
    void cleanup_deferred_destroys();
};

} // namespace goggles::render
