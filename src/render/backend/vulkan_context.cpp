#include "vulkan_context.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <format>
#include <util/logging.hpp>
#include <utility>
#include <vector>

namespace goggles::render::backend_internal {

namespace {

constexpr std::array REQUIRED_INSTANCE_EXTENSIONS = {
    VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
};

constexpr const char* VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

constexpr std::array REQUIRED_DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,          VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,  VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
};

constexpr std::array OPTIONAL_DEVICE_EXTENSIONS = {
    VK_KHR_PRESENT_ID_EXTENSION_NAME,
    VK_KHR_PRESENT_WAIT_EXTENSION_NAME,
};

struct PhysicalDeviceCandidate {
    vk::PhysicalDevice device;
    uint32_t graphics_family = UINT32_MAX;
    uint32_t index = 0;
    bool present_wait_supported = false;
    int score = 0;
};

auto to_ascii_lower(std::string value) -> std::string {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

auto get_device_name(vk::PhysicalDevice device) -> std::string {
    return device.getProperties().deviceName.data();
}

auto has_string_extension(const std::vector<const char*>& extensions, const char* required)
    -> bool {
    return std::ranges::any_of(extensions, [required](const char* extension) {
        return std::strcmp(extension, required) == 0;
    });
}

auto has_device_extension(const std::vector<vk::ExtensionProperties>& extensions,
                          const char* required) -> bool {
    return std::ranges::any_of(extensions, [required](const vk::ExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, required) == 0;
    });
}

auto make_application_info() -> vk::ApplicationInfo {
    vk::ApplicationInfo app_info{};
    app_info.pApplicationName = "Goggles";
    app_info.applicationVersion =
        VK_MAKE_VERSION(GOGGLES_VERSION_MAJOR, GOGGLES_VERSION_MINOR, GOGGLES_VERSION_PATCH);
    app_info.pEngineName = "Goggles";
    app_info.engineVersion =
        VK_MAKE_VERSION(GOGGLES_VERSION_MAJOR, GOGGLES_VERSION_MINOR, GOGGLES_VERSION_PATCH);
    app_info.apiVersion = VK_API_VERSION_1_3;
    return app_info;
}

auto create_instance(VulkanContext& context, std::vector<const char*> extensions) -> Result<void> {
    for (const auto* extension : REQUIRED_INSTANCE_EXTENSIONS) {
        if (!has_string_extension(extensions, extension)) {
            extensions.push_back(extension);
        }
    }

    std::vector<const char*> layers;
    if (context.enable_validation) {
        if (is_validation_layer_available()) {
            layers.push_back(VALIDATION_LAYER_NAME);
            if (!has_string_extension(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
            GOGGLES_LOG_INFO("Vulkan validation layer enabled");
        } else {
            GOGGLES_LOG_WARN("Vulkan validation layer requested but not available");
        }
    }

    auto app_info = make_application_info();

    vk::InstanceCreateInfo create_info{};
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.data();

    auto [result, instance] = vk::createInstance(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create Vulkan instance: " + vk::to_string(result));
    }

    context.instance = instance;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(context.instance);

    GOGGLES_LOG_DEBUG("Vulkan instance created with {} extensions, {} layers", extensions.size(),
                      layers.size());
    return {};
}

auto create_debug_messenger(VulkanContext& context) -> Result<void> {
    if (!context.enable_validation || !is_validation_layer_available()) {
        return {};
    }

    auto messenger_result = VulkanDebugMessenger::create(context.instance);
    if (!messenger_result) {
        GOGGLES_LOG_WARN("Failed to create debug messenger: {}", messenger_result.error().message);
        return {};
    }

    context.debug_messenger = std::move(messenger_result.value());
    return {};
}

auto create_surface(VulkanContext& context, SDL_Window* window) -> Result<void> {
    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, context.instance, nullptr, &raw_surface)) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }

    context.surface = raw_surface;
    GOGGLES_LOG_DEBUG("Vulkan surface created");
    return {};
}

auto select_candidate_by_gpu_selector(const std::vector<PhysicalDeviceCandidate>& candidates,
                                      const std::string& gpu_selector,
                                      const std::string& available_gpus)
    -> Result<const PhysicalDeviceCandidate*> {
    uint32_t selector_index = 0;
    const auto parse_end = gpu_selector.data() + gpu_selector.size();
    auto [parsed_end, parsed_ec] = std::from_chars(gpu_selector.data(), parse_end, selector_index);
    const bool numeric_selector = parsed_ec == std::errc{} && parsed_end == parse_end;

    if (numeric_selector) {
        auto it = std::find_if(candidates.begin(), candidates.end(),
                               [selector_index](const PhysicalDeviceCandidate& candidate) {
                                   return candidate.index == selector_index;
                               });
        if (it == candidates.end()) {
            return make_error<const PhysicalDeviceCandidate*>(
                ErrorCode::vulkan_init_failed,
                "GPU selector '" + gpu_selector +
                    "' is invalid or unsuitable. Available GPUs: " + available_gpus);
        }
        return &*it;
    }

    const auto selector_lower = to_ascii_lower(gpu_selector);
    std::vector<const PhysicalDeviceCandidate*> matched_candidates;
    for (const auto& candidate : candidates) {
        const auto device_name = to_ascii_lower(get_device_name(candidate.device));
        if (device_name.find(selector_lower) != std::string::npos) {
            matched_candidates.push_back(&candidate);
        }
    }

    if (matched_candidates.empty()) {
        return make_error<const PhysicalDeviceCandidate*>(
            ErrorCode::vulkan_init_failed,
            "GPU selector '" + gpu_selector +
                "' is invalid or unsuitable. Available GPUs: " + available_gpus);
    }

    if (matched_candidates.size() > 1) {
        std::string matched_gpus;
        for (const auto* candidate : matched_candidates) {
            const auto gpu_name = get_device_name(candidate->device);
            matched_gpus += std::format("{}[{}] {}", matched_gpus.empty() ? "" : ", ",
                                        candidate->index, gpu_name);
        }
        return make_error<const PhysicalDeviceCandidate*>(
            ErrorCode::vulkan_init_failed, "GPU selector '" + gpu_selector +
                                               "' matches multiple suitable GPUs: " + matched_gpus +
                                               ". Please use a numeric index.");
    }

    return matched_candidates.front();
}

void populate_selected_device_details(VulkanContext& context,
                                      const PhysicalDeviceCandidate& selected, bool headless_log) {
    context.physical_device = selected.device;
    context.graphics_queue_family = selected.graphics_family;
    context.gpu_index = selected.index;
    context.present_wait_supported = headless_log ? false : selected.present_wait_supported;

    vk::PhysicalDeviceIDProperties id_props{};
    vk::PhysicalDeviceProperties2 props2{};
    props2.pNext = &id_props;
    context.physical_device.getProperties2(&props2);

    std::array<char, 37> uuid_hex{};
    std::snprintf(uuid_hex.data(), uuid_hex.size(),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  id_props.deviceUUID[0], id_props.deviceUUID[1], id_props.deviceUUID[2],
                  id_props.deviceUUID[3], id_props.deviceUUID[4], id_props.deviceUUID[5],
                  id_props.deviceUUID[6], id_props.deviceUUID[7], id_props.deviceUUID[8],
                  id_props.deviceUUID[9], id_props.deviceUUID[10], id_props.deviceUUID[11],
                  id_props.deviceUUID[12], id_props.deviceUUID[13], id_props.deviceUUID[14],
                  id_props.deviceUUID[15]);
    context.gpu_uuid = uuid_hex.data();

    if (headless_log) {
        GOGGLES_LOG_INFO("Selected GPU (headless): {} (UUID: {})",
                         props2.properties.deviceName.data(), context.gpu_uuid);
    } else {
        GOGGLES_LOG_INFO("Selected GPU: {} (UUID: {})", props2.properties.deviceName.data(),
                         context.gpu_uuid);
    }
}

auto select_physical_device(VulkanContext& context, const std::string& gpu_selector)
    -> Result<void> {
    auto [result, devices] = context.instance.enumeratePhysicalDevices();
    if (result != vk::Result::eSuccess || devices.empty()) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "No Vulkan devices found");
    }

    std::vector<PhysicalDeviceCandidate> candidates;
    std::string available_gpus;

    GOGGLES_LOG_INFO("Available GPUs:");
    for (size_t idx = 0; idx < devices.size(); ++idx) {
        const auto& device = devices[idx];
        auto props = device.getProperties();
        auto queue_families = device.getQueueFamilyProperties();
        uint32_t graphics_family = UINT32_MAX;

        for (uint32_t family_index = 0; family_index < queue_families.size(); ++family_index) {
            const auto& family = queue_families[family_index];
            if (family.queueFlags & vk::QueueFlagBits::eGraphics) {
                auto [support_result, supported] =
                    device.getSurfaceSupportKHR(family_index, context.surface);
                if (support_result == vk::Result::eSuccess && supported) {
                    graphics_family = family_index;
                    break;
                }
            }
        }

        const bool surface_ok = graphics_family != UINT32_MAX;
        bool extensions_ok = false;
        bool present_wait_supported = false;

        if (surface_ok) {
            auto [ext_result, available_extensions] = device.enumerateDeviceExtensionProperties();
            if (ext_result == vk::Result::eSuccess) {
                extensions_ok = true;
                for (const auto* required : REQUIRED_DEVICE_EXTENSIONS) {
                    if (!has_device_extension(available_extensions, required)) {
                        extensions_ok = false;
                        break;
                    }
                }

                if (extensions_ok) {
                    const bool present_id_ok = has_device_extension(
                        available_extensions, VK_KHR_PRESENT_ID_EXTENSION_NAME);
                    const bool present_wait_ok = has_device_extension(
                        available_extensions, VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
                    present_wait_supported = present_id_ok && present_wait_ok;
                }
            }
        }

        const char* status = (surface_ok && extensions_ok)
                                 ? "suitable"
                                 : (!surface_ok ? "no surface support" : "missing extensions");
        GOGGLES_LOG_INFO("  [{}] {} ({})", idx, props.deviceName.data(), status);
        available_gpus += std::format("{}[{}] {}", available_gpus.empty() ? "" : ", ", idx,
                                      props.deviceName.data());

        if (surface_ok && extensions_ok) {
            int score = 0;
            if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                score += 1000;
            } else if (props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
                score += 500;
            }
            candidates.push_back({.device = device,
                                  .graphics_family = graphics_family,
                                  .index = static_cast<uint32_t>(idx),
                                  .present_wait_supported = present_wait_supported,
                                  .score = score});
        }
    }

    if (candidates.empty()) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "No suitable GPU found with DMA-BUF and surface support");
    }

    const PhysicalDeviceCandidate* selected = nullptr;
    if (!gpu_selector.empty()) {
        auto selected_result =
            select_candidate_by_gpu_selector(candidates, gpu_selector, available_gpus);
        if (!selected_result) {
            return make_error<void>(selected_result.error().code, selected_result.error().message,
                                    selected_result.error().location);
        }
        selected = selected_result.value();
    } else {
        auto best = std::max_element(
            candidates.begin(), candidates.end(),
            [](const PhysicalDeviceCandidate& lhs, const PhysicalDeviceCandidate& rhs) {
                return lhs.score < rhs.score;
            });
        selected = &*best;
    }

    populate_selected_device_details(context, *selected, false);
    return {};
}

auto select_physical_device_headless(VulkanContext& context, const std::string& gpu_selector)
    -> Result<void> {
    auto [result, devices] = context.instance.enumeratePhysicalDevices();
    if (result != vk::Result::eSuccess || devices.empty()) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "No Vulkan devices found");
    }

    std::vector<const char*> headless_required_extensions;
    headless_required_extensions.reserve(REQUIRED_DEVICE_EXTENSIONS.size());
    for (const auto* extension : REQUIRED_DEVICE_EXTENSIONS) {
        if (std::strcmp(extension, VK_KHR_SWAPCHAIN_EXTENSION_NAME) != 0) {
            headless_required_extensions.push_back(extension);
        }
    }

    std::vector<PhysicalDeviceCandidate> candidates;
    std::string available_gpus;

    GOGGLES_LOG_INFO("Available GPUs (headless):");
    for (size_t idx = 0; idx < devices.size(); ++idx) {
        const auto& device = devices[idx];
        auto props = device.getProperties();
        auto queue_families = device.getQueueFamilyProperties();
        uint32_t graphics_family = UINT32_MAX;

        for (uint32_t family_index = 0; family_index < queue_families.size(); ++family_index) {
            if (queue_families[family_index].queueFlags & vk::QueueFlagBits::eGraphics) {
                graphics_family = family_index;
                break;
            }
        }

        const bool graphics_ok = graphics_family != UINT32_MAX;
        bool extensions_ok = false;

        if (graphics_ok) {
            auto [ext_result, available_extensions] = device.enumerateDeviceExtensionProperties();
            if (ext_result == vk::Result::eSuccess) {
                extensions_ok = true;
                for (const auto* required : headless_required_extensions) {
                    if (!has_device_extension(available_extensions, required)) {
                        extensions_ok = false;
                        break;
                    }
                }
            }
        }

        const char* status = (graphics_ok && extensions_ok)
                                 ? "suitable"
                                 : (!graphics_ok ? "no graphics queue" : "missing extensions");
        GOGGLES_LOG_INFO("  [{}] {} ({})", idx, props.deviceName.data(), status);
        available_gpus += std::format("{}[{}] {}", available_gpus.empty() ? "" : ", ", idx,
                                      props.deviceName.data());

        if (graphics_ok && extensions_ok) {
            int score = 0;
            if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                score += 1000;
            } else if (props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
                score += 500;
            }
            candidates.push_back({.device = device,
                                  .graphics_family = graphics_family,
                                  .index = static_cast<uint32_t>(idx),
                                  .present_wait_supported = false,
                                  .score = score});
        }
    }

    if (candidates.empty()) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "No suitable GPU found with DMA-BUF support (headless)");
    }

    const PhysicalDeviceCandidate* selected = nullptr;
    if (!gpu_selector.empty()) {
        auto selected_result =
            select_candidate_by_gpu_selector(candidates, gpu_selector, available_gpus);
        if (!selected_result) {
            return make_error<void>(selected_result.error().code, selected_result.error().message,
                                    selected_result.error().location);
        }
        selected = selected_result.value();
    } else {
        auto best = std::max_element(
            candidates.begin(), candidates.end(),
            [](const PhysicalDeviceCandidate& lhs, const PhysicalDeviceCandidate& rhs) {
                return lhs.score < rhs.score;
            });
        selected = &*best;
    }

    populate_selected_device_details(context, *selected, true);
    return {};
}

auto create_device(VulkanContext& context) -> Result<void> {
    float queue_priority = 1.0F;
    vk::DeviceQueueCreateInfo queue_info{};
    queue_info.queueFamilyIndex = context.graphics_queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    vk::PhysicalDeviceVulkan11Features vk11_features{};
    vk::PhysicalDeviceVulkan12Features vk12_features{};
    vk::PhysicalDeviceVulkan13Features vk13_features{};
    vk::PhysicalDevicePresentIdFeaturesKHR present_id_features{};
    vk::PhysicalDevicePresentWaitFeaturesKHR present_wait_features{};
    vk11_features.pNext = &vk12_features;
    vk12_features.pNext = &vk13_features;

    if (context.present_wait_supported) {
        vk13_features.pNext = &present_id_features;
        present_id_features.pNext = &present_wait_features;
    }

    vk::PhysicalDeviceFeatures2 features2{};
    features2.pNext = &vk11_features;
    context.physical_device.getFeatures2(&features2);

    const bool present_wait_features_ok = (present_id_features.presentId != VK_FALSE) &&
                                          (present_wait_features.presentWait != VK_FALSE);
    const bool present_wait_ready = context.present_wait_supported && present_wait_features_ok;
    if (context.present_wait_supported && !present_wait_ready) {
        GOGGLES_LOG_WARN(
            "VK_KHR_present_id/VK_KHR_present_wait extensions present but features disabled; "
            "falling back to mailbox/throttle");
    }
    context.present_wait_supported = present_wait_ready;

    if (!vk11_features.shaderDrawParameters) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "shaderDrawParameters not supported (required for vertex shaders)");
    }
    if (!vk13_features.dynamicRendering) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Dynamic rendering not supported (required for Vulkan 1.3)");
    }

    vk::PhysicalDeviceVulkan11Features vk11_enable{};
    vk11_enable.shaderDrawParameters = VK_TRUE;
    vk::PhysicalDeviceVulkan12Features vk12_enable{};
    vk::PhysicalDeviceVulkan13Features vk13_enable{};
    vk13_enable.dynamicRendering = VK_TRUE;
    vk::PhysicalDevicePresentIdFeaturesKHR present_id_enable{};
    vk::PhysicalDevicePresentWaitFeaturesKHR present_wait_enable{};
    vk11_enable.pNext = &vk12_enable;
    vk12_enable.pNext = &vk13_enable;
    if (context.present_wait_supported) {
        present_id_enable.presentId = VK_TRUE;
        present_wait_enable.presentWait = VK_TRUE;
        vk13_enable.pNext = &present_id_enable;
        present_id_enable.pNext = &present_wait_enable;
    }

    std::array<const char*, REQUIRED_DEVICE_EXTENSIONS.size() + OPTIONAL_DEVICE_EXTENSIONS.size()>
        extensions{};
    size_t extension_count = 0;
    for (const auto* extension : REQUIRED_DEVICE_EXTENSIONS) {
        if (context.headless && std::strcmp(extension, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            continue;
        }
        extensions[extension_count++] = extension;
    }
    if (!context.headless && context.present_wait_supported) {
        for (const auto* extension : OPTIONAL_DEVICE_EXTENSIONS) {
            extensions[extension_count++] = extension;
        }
    }

    vk::DeviceCreateInfo create_info{};
    create_info.pNext = &vk11_enable;
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extension_count);
    create_info.ppEnabledExtensionNames = extensions.data();

    auto [result, device] = context.physical_device.createDevice(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create logical device: " + vk::to_string(result));
    }

    context.device = device;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(context.device);
    context.graphics_queue = context.device.getQueue(context.graphics_queue_family, 0);

    GOGGLES_LOG_DEBUG("Vulkan device created");
    return {};
}

} // namespace

VulkanContext::VulkanContext(VulkanContext&& other) noexcept {
    *this = std::move(other);
}

auto VulkanContext::operator=(VulkanContext&& other) noexcept -> VulkanContext& {
    if (this == &other) {
        return *this;
    }

    destroy();

    instance = std::exchange(other.instance, nullptr);
    physical_device = std::exchange(other.physical_device, nullptr);
    device = std::exchange(other.device, nullptr);
    graphics_queue = std::exchange(other.graphics_queue, nullptr);
    surface = std::exchange(other.surface, nullptr);
    debug_messenger = std::move(other.debug_messenger);
    other.debug_messenger.reset();
    graphics_queue_family = std::exchange(other.graphics_queue_family, UINT32_MAX);
    gpu_index = std::exchange(other.gpu_index, 0u);
    gpu_uuid = std::move(other.gpu_uuid);
    other.gpu_uuid.clear();
    enable_validation = std::exchange(other.enable_validation, false);
    headless = std::exchange(other.headless, false);
    present_wait_supported = std::exchange(other.present_wait_supported, false);

    return *this;
}

auto VulkanContext::create(SDL_Window* window, bool enable_validation,
                           const std::string& gpu_selector) -> Result<VulkanContext> {
    if (window == nullptr) {
        return make_error<VulkanContext>(ErrorCode::vulkan_init_failed,
                                         "Cannot create Vulkan context without a window");
    }

    auto vk_get_instance_proc_addr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
    if (vk_get_instance_proc_addr == nullptr) {
        return make_error<VulkanContext>(ErrorCode::vulkan_init_failed,
                                         "Failed to get vkGetInstanceProcAddr from SDL");
    }
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk_get_instance_proc_addr);

    VulkanContext context{};
    context.enable_validation = enable_validation;

    uint32_t sdl_extension_count = 0;
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
    if (sdl_extensions == nullptr) {
        return make_error<VulkanContext>(ErrorCode::vulkan_init_failed,
                                         std::string("SDL_Vulkan_GetInstanceExtensions failed: ") +
                                             SDL_GetError());
    }

    std::vector<const char*> extensions(sdl_extensions, sdl_extensions + sdl_extension_count);

    auto instance_result = create_instance(context, std::move(extensions));
    if (!instance_result) {
        context.destroy();
        return make_error<VulkanContext>(instance_result.error().code,
                                         instance_result.error().message,
                                         instance_result.error().location);
    }

    auto debug_result = create_debug_messenger(context);
    if (!debug_result) {
        context.destroy();
        return make_error<VulkanContext>(debug_result.error().code, debug_result.error().message,
                                         debug_result.error().location);
    }

    auto surface_result = create_surface(context, window);
    if (!surface_result) {
        context.destroy();
        return make_error<VulkanContext>(surface_result.error().code,
                                         surface_result.error().message,
                                         surface_result.error().location);
    }

    auto select_result = select_physical_device(context, gpu_selector);
    if (!select_result) {
        context.destroy();
        return make_error<VulkanContext>(select_result.error().code, select_result.error().message,
                                         select_result.error().location);
    }

    auto device_result = create_device(context);
    if (!device_result) {
        context.destroy();
        return make_error<VulkanContext>(device_result.error().code, device_result.error().message,
                                         device_result.error().location);
    }

    return context;
}

auto VulkanContext::create_headless(bool enable_validation, const std::string& gpu_selector)
    -> Result<VulkanContext> {
    if (!VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr) {
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    }

    VulkanContext context{};
    context.enable_validation = enable_validation;
    context.headless = true;

    std::vector<const char*> extensions;
    extensions.reserve(REQUIRED_INSTANCE_EXTENSIONS.size() + 1u);

    auto instance_result = create_instance(context, std::move(extensions));
    if (!instance_result) {
        context.destroy();
        return make_error<VulkanContext>(instance_result.error().code,
                                         instance_result.error().message,
                                         instance_result.error().location);
    }

    auto debug_result = create_debug_messenger(context);
    if (!debug_result) {
        context.destroy();
        return make_error<VulkanContext>(debug_result.error().code, debug_result.error().message,
                                         debug_result.error().location);
    }

    auto select_result = select_physical_device_headless(context, gpu_selector);
    if (!select_result) {
        context.destroy();
        return make_error<VulkanContext>(select_result.error().code, select_result.error().message,
                                         select_result.error().location);
    }

    auto device_result = create_device(context);
    if (!device_result) {
        context.destroy();
        return make_error<VulkanContext>(device_result.error().code, device_result.error().message,
                                         device_result.error().location);
    }

    return context;
}

void VulkanContext::destroy() {
    if (device) {
        device.destroy();
        device = nullptr;
    }
    graphics_queue = nullptr;
    physical_device = nullptr;

    if (instance && surface) {
        instance.destroySurfaceKHR(surface);
        surface = nullptr;
    }

    debug_messenger.reset();

    if (instance) {
        instance.destroy();
        instance = nullptr;
    }

    graphics_queue_family = UINT32_MAX;
    gpu_index = 0;
    gpu_uuid.clear();
    enable_validation = false;
    headless = false;
    present_wait_supported = false;
}

auto VulkanContext::boundary_context(vk::CommandPool command_pool) const
    -> ::goggles::render::VulkanContext {
    return ::goggles::render::VulkanContext{
        .device = device,
        .physical_device = physical_device,
        .command_pool = command_pool,
        .graphics_queue = graphics_queue,
        .graphics_queue_family_index = graphics_queue_family,
    };
}

} // namespace goggles::render::backend_internal
