#pragma once

#include <cstdint>
#include <goggles_filter_chain.h>
#include <string>

namespace goggles::filter_chain::runtime {

class Instance;

/// @brief Internal device implementation backing `goggles_fc_device_t`.
///
/// Binds borrowed VkPhysicalDevice/VkDevice/VkQueue inputs while owning
/// device-scoped caches, setup/upload command resources, and teardown ordering.
class Device {
public:
    [[nodiscard]] static auto create(Instance* instance,
                                     const goggles_fc_vk_device_create_info_t* create_info,
                                     goggles_fc_device_t** out_device) -> goggles_fc_status_t;

    ~Device();

    Device(const Device&) = delete;
    auto operator=(const Device&) -> Device& = delete;
    Device(Device&&) = delete;
    auto operator=(Device&&) -> Device& = delete;

    /// @brief Return the parent instance.
    [[nodiscard]] auto instance() const -> Instance* { return m_instance; }

    /// @brief Return the borrowed Vulkan physical device.
    [[nodiscard]] auto physical_device() const -> VkPhysicalDevice { return m_physical_device; }

    /// @brief Return the borrowed Vulkan device.
    [[nodiscard]] auto vk_device() const -> VkDevice { return m_device; }

    /// @brief Return the borrowed Vulkan queue.
    [[nodiscard]] auto graphics_queue() const -> VkQueue { return m_graphics_queue; }

    /// @brief Return the queue family index.
    [[nodiscard]] auto graphics_queue_family_index() const -> uint32_t {
        return m_graphics_queue_family_index;
    }

    /// @brief Return the optional cache directory path.
    [[nodiscard]] auto cache_dir() const -> const std::string& { return m_cache_dir; }

    /// @brief Return the owned setup/upload command pool.
    [[nodiscard]] auto setup_command_pool() const -> VkCommandPool { return m_setup_command_pool; }

    /// @brief Return the owned setup/upload fence.
    [[nodiscard]] auto setup_fence() const -> VkFence { return m_setup_fence; }

    /// @brief Return the owned pipeline cache (may be VK_NULL_HANDLE if creation failed).
    [[nodiscard]] auto pipeline_cache() const -> VkPipelineCache { return m_pipeline_cache; }

    /// @brief Return the raw pointer suitable for casting to goggles_fc_device_t*.
    [[nodiscard]] auto as_handle() -> goggles_fc_device_t*;

    /// @brief Recover the Device from an opaque handle.
    [[nodiscard]] static auto from_handle(goggles_fc_device_t* handle) -> Device*;
    [[nodiscard]] static auto from_handle(const goggles_fc_device_t* handle) -> const Device*;

    /// @brief Check whether the opaque handle points to a live Device (magic validation).
    [[nodiscard]] static auto check_magic(const void* handle) -> bool {
        if (handle == nullptr) {
            return false;
        }
        return static_cast<const Device*>(handle)->m_magic == DEVICE_MAGIC;
    }

private:
    Device() = default;

    Instance* m_instance = nullptr;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    uint32_t m_graphics_queue_family_index = 0;
    std::string m_cache_dir;

    // Owned device-scoped resources for setup/upload operations.
    VkCommandPool m_setup_command_pool = VK_NULL_HANDLE;
    VkFence m_setup_fence = VK_NULL_HANDLE;
    VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;

    static constexpr uint32_t DEVICE_MAGIC = 0x47464344u; // "GFCD"
    uint32_t m_magic = DEVICE_MAGIC;
};

} // namespace goggles::filter_chain::runtime
