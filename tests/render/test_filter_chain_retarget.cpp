#include "render/backend/filter_chain_controller.hpp"

#include <atomic>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <util/logging.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace {

constexpr uint32_t TEST_SYNC_INDICES = 2u;

auto make_cache_dir() -> std::filesystem::path {
    static std::atomic<uint64_t> counter{0u};
    auto path = std::filesystem::temp_directory_path() /
                ("goggles_filter_chain_retarget_" + std::to_string(counter.fetch_add(1u)));
    std::filesystem::create_directories(path);
    return path;
}

auto read_text_file(const std::filesystem::path& path) -> std::optional<std::string> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

struct VulkanRuntimeFixture {
    VulkanRuntimeFixture() {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "goggles_filter_chain_retarget_tests";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo instance_info{};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo = &app_info;
        if (vkCreateInstance(&instance_info, nullptr, &m_instance) != VK_SUCCESS) {
            return;
        }
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance{m_instance});

        uint32_t device_count = 0u;
        if (vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr) != VK_SUCCESS ||
            device_count == 0u) {
            return;
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        if (vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data()) != VK_SUCCESS) {
            return;
        }

        for (const auto candidate : devices) {
            uint32_t family_count = 0u;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
            if (family_count == 0u) {
                continue;
            }

            std::vector<VkQueueFamilyProperties> families(family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());
            for (uint32_t family = 0u; family < family_count; ++family) {
                if ((families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0u) {
                    continue;
                }

                m_physical_device = candidate;
                m_queue_family_index = family;
                goto found_queue;
            }
        }

    found_queue:
        if (m_physical_device == VK_NULL_HANDLE) {
            return;
        }

        float queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = m_queue_family_index;
        queue_info.queueCount = 1u;
        queue_info.pQueuePriorities = &queue_priority;

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &features13;
        vkGetPhysicalDeviceFeatures2(m_physical_device, &features2);
        if (features13.dynamicRendering != VK_TRUE) {
            return;
        }
        features13.dynamicRendering = VK_TRUE;

        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1u;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.pNext = &features13;
        if (vkCreateDevice(m_physical_device, &device_info, nullptr, &m_device) != VK_SUCCESS) {
            return;
        }

        vkGetDeviceQueue(m_device, m_queue_family_index, 0u, &m_queue);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Device{m_device});
    }

    ~VulkanRuntimeFixture() {
        if (m_device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device);
            vkDestroyDevice(m_device, nullptr);
        }
        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
        }
    }

    [[nodiscard]] auto available() const -> bool { return m_device != VK_NULL_HANDLE; }

    [[nodiscard]] auto device_info() const
        -> goggles::render::backend_internal::FilterChainController::VulkanDeviceInfo {
        return goggles::render::backend_internal::FilterChainController::VulkanDeviceInfo{
            .physical_device = m_physical_device,
            .device = m_device,
            .graphics_queue = m_queue,
            .graphics_queue_family_index = m_queue_family_index,
            .cache_dir = {},
        };
    }

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_queue_family_index = UINT32_MAX;
};

struct CacheDirGuard {
    explicit CacheDirGuard(std::filesystem::path path) : dir(std::move(path)) {}
    ~CacheDirGuard() {
        if (!dir.empty()) {
            std::filesystem::remove_all(dir);
        }
    }

    std::filesystem::path dir;
};

auto find_filter_control(const std::vector<goggles::fc::FilterControlDescriptor>& controls,
                         std::string_view name) -> const goggles::fc::FilterControlDescriptor* {
    for (const auto& control : controls) {
        if (control.name == name) {
            return &control;
        }
    }
    return nullptr;
}

struct TestBuildConfig {
    goggles::render::backend_internal::FilterChainController::VulkanDeviceInfo device_info;
    goggles::render::backend_internal::FilterChainController::ChainConfig chain_config;
};

auto make_adapter_build_config(const VulkanRuntimeFixture& fixture,
                               const std::filesystem::path& cache_dir,
                               VkFormat target_format = VK_FORMAT_B8G8R8A8_UNORM)
    -> TestBuildConfig {
    auto dev_info = fixture.device_info();
    dev_info.cache_dir = cache_dir.string();
    return {
        .device_info = dev_info,
        .chain_config =
            goggles::render::backend_internal::FilterChainController::ChainConfig{
                .target_format = target_format,
                .frames_in_flight = TEST_SYNC_INDICES,
                .initial_prechain_width = 1u,
                .initial_prechain_height = 1u,
            },
    };
}

void configure_controller_runtime(
    goggles::render::backend_internal::FilterChainController& controller,
    const std::filesystem::path& preset_path) {
    controller.preset_path = preset_path;
    controller.set_stage_policy(true, false);
    controller.set_prechain_resolution(vk::Extent2D{2u, 3u});

    const auto controls =
        controller.list_filter_controls(goggles::fc::FilterControlStage::prechain);
    const auto* filter_type = find_filter_control(controls, "filter_type");
    REQUIRE(filter_type != nullptr);
    REQUIRE(controller.set_filter_control_value(filter_type->control_id, 2.0F));
}

void require_controller_state(goggles::render::backend_internal::FilterChainController& controller,
                              const std::filesystem::path& preset_path) {
    REQUIRE(controller.current_preset_path() == preset_path);
    REQUIRE(controller.current_prechain_resolution() == vk::Extent2D{2u, 3u});

    // Stage policy is stored as boolean flags on the controller.
    REQUIRE(controller.prechain_policy_enabled);
    REQUIRE(!controller.effect_stage_policy_enabled);

    const auto controls =
        controller.list_filter_controls(goggles::fc::FilterControlStage::prechain);
    const auto* filter_type = find_filter_control(controls, "filter_type");
    REQUIRE(filter_type != nullptr);
    REQUIRE(filter_type->current_value == Catch::Approx(2.0F));

    auto report_result = controller.get_chain_report();
    REQUIRE(report_result.has_value());
    REQUIRE(report_result->current_stage_mask ==
            (GOGGLES_FC_STAGE_MASK_PRECHAIN | GOGGLES_FC_STAGE_MASK_POSTCHAIN));
}

void wait_for_reload_start(
    const goggles::render::backend_internal::FilterChainController& controller) {
    using namespace std::chrono_literals;

    for (int attempt = 0; attempt < 200; ++attempt) {
        if (controller.pending_chain_ready.load(std::memory_order_acquire)) {
            return;
        }
        if (controller.pending_load_future.valid() &&
            controller.pending_load_future.wait_for(0ms) == std::future_status::ready) {
            break;
        }
        std::this_thread::sleep_for(10ms);
    }

    FAIL("Timed out waiting for pending shader reload to become observable");
}

} // namespace

TEST_CASE("Controller retarget preserves active runtime without swap signaling",
          "[filter_chain][retarget][runtime]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed controller retarget test because no Vulkan graphics device is "
             "available");
    }

    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    auto controller = goggles::render::backend_internal::FilterChainController{};
    auto build_config = make_adapter_build_config(fixture, cache_dir_guard.dir);

    REQUIRE(controller.recreate_filter_chain(build_config.device_info, build_config.chain_config)
                .has_value());
    configure_controller_runtime(controller, preset_path);

    REQUIRE_FALSE(controller.consume_chain_swapped());
    REQUIRE(controller
                .retarget_filter_chain(
                    {.format = vk::Format::eB8G8R8A8Srgb, .extent = vk::Extent2D{4u, 5u}})
                .has_value());
    REQUIRE_FALSE(controller.consume_chain_swapped());
    require_controller_state(controller, preset_path);

    controller.shutdown([&fixture]() { vkDeviceWaitIdle(fixture.device_info().device); });
}

TEST_CASE("Controller retarget failure keeps the previous runtime usable",
          "[filter_chain][retarget][runtime]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed controller retarget failure test because no Vulkan graphics "
             "device is available");
    }

    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    auto controller = goggles::render::backend_internal::FilterChainController{};
    auto build_config = make_adapter_build_config(fixture, cache_dir_guard.dir);

    REQUIRE(controller.recreate_filter_chain(build_config.device_info, build_config.chain_config)
                .has_value());
    configure_controller_runtime(controller, preset_path);

    const auto invalid_retarget = controller.retarget_filter_chain(
        {.format = vk::Format::eUndefined, .extent = vk::Extent2D{4u, 5u}});
    REQUIRE_FALSE(invalid_retarget.has_value());
    REQUIRE_FALSE(controller.consume_chain_swapped());
    require_controller_state(controller, preset_path);
    REQUIRE(controller
                .retarget_filter_chain(
                    {.format = vk::Format::eB8G8R8A8Srgb, .extent = vk::Extent2D{4u, 5u}})
                .has_value());

    controller.shutdown([&fixture]() { vkDeviceWaitIdle(fixture.device_info().device); });
}

TEST_CASE("Pending reload swaps only after activation and preserves authoritative state",
          "[filter_chain][retarget][runtime]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP(
            "Skipping Vulkan-backed pending reload retarget test because no Vulkan graphics device "
            "is available");
    }

    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    auto controller = goggles::render::backend_internal::FilterChainController{};
    auto build_config = make_adapter_build_config(fixture, cache_dir_guard.dir);

    REQUIRE(controller.recreate_filter_chain(build_config.device_info, build_config.chain_config)
                .has_value());
    configure_controller_runtime(controller, preset_path);

    REQUIRE(
        controller
            .reload_shader_preset(preset_path, build_config.device_info, build_config.chain_config)
            .has_value());
    wait_for_reload_start(controller);
    REQUIRE(controller.pending_chain_ready.load(std::memory_order_acquire));
    REQUIRE_FALSE(controller.consume_chain_swapped());

    REQUIRE(controller
                .retarget_filter_chain(
                    {.format = vk::Format::eB8G8R8A8Srgb, .extent = vk::Extent2D{4u, 5u}})
                .has_value());
    REQUIRE_FALSE(controller.consume_chain_swapped());

    controller.check_pending_chain_swap([] {});
    REQUIRE(controller.consume_chain_swapped());
    REQUIRE_FALSE(controller.pending_chain_ready.load(std::memory_order_acquire));
    require_controller_state(controller, preset_path);

    controller.shutdown([&fixture]() { vkDeviceWaitIdle(fixture.device_info().device); });
}

TEST_CASE("Explicit reload failure preserves the previous runtime",
          "[filter_chain][retarget][runtime]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed reload failure test because no Vulkan graphics device is "
             "available");
    }

    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    const auto missing_preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/missing.slangp";
    REQUIRE(std::filesystem::exists(preset_path));
    REQUIRE_FALSE(std::filesystem::exists(missing_preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    auto controller = goggles::render::backend_internal::FilterChainController{};
    auto build_config = make_adapter_build_config(fixture, cache_dir_guard.dir);

    REQUIRE(controller.recreate_filter_chain(build_config.device_info, build_config.chain_config)
                .has_value());
    configure_controller_runtime(controller, preset_path);

    REQUIRE(controller
                .reload_shader_preset(missing_preset_path, build_config.device_info,
                                      build_config.chain_config)
                .has_value());

    using namespace std::chrono_literals;
    REQUIRE(controller.pending_load_future.valid());
    REQUIRE(controller.pending_load_future.wait_for(5s) == std::future_status::ready);
    const auto reload_result = controller.pending_load_future.get();
    REQUIRE_FALSE(reload_result.has_value());
    REQUIRE_FALSE(controller.pending_chain_ready.load(std::memory_order_acquire));
    REQUIRE_FALSE(controller.consume_chain_swapped());
    require_controller_state(controller, preset_path);
    REQUIRE(controller
                .retarget_filter_chain(
                    {.format = vk::Format::eB8G8R8A8Srgb, .extent = vk::Extent2D{4u, 5u}})
                .has_value());

    controller.shutdown([&fixture]() { vkDeviceWaitIdle(fixture.device_info().device); });
}

TEST_CASE("Reload across different control surfaces skips stale restore warnings",
          "[filter_chain][retarget][runtime]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed reload warning test because no Vulkan graphics device is "
             "available");
    }

    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    CacheDirGuard log_dir_guard(make_cache_dir());
    const auto log_path = log_dir_guard.dir / "filter_chain_reload.log";

    goggles::initialize_logger("filter_chain_retarget_tests");
    goggles::set_log_level(spdlog::level::warn);
    REQUIRE(goggles::set_log_file_path(log_path).has_value());

    auto controller = goggles::render::backend_internal::FilterChainController{};
    auto build_config = make_adapter_build_config(fixture, cache_dir_guard.dir);

    REQUIRE(controller.recreate_filter_chain(build_config.device_info, build_config.chain_config)
                .has_value());
    controller.set_stage_policy(true, true);
    controller.set_prechain_resolution(vk::Extent2D{2u, 3u});
    controller.load_shader_preset(preset_path);

    const auto prechain_controls_before =
        controller.list_filter_controls(goggles::fc::FilterControlStage::prechain);
    const auto* filter_type_before = find_filter_control(prechain_controls_before, "filter_type");
    REQUIRE(filter_type_before != nullptr);
    REQUIRE(controller.set_filter_control_value(filter_type_before->control_id, 2.0F));
    controller.authoritative_control_overrides.push_back(
        {.control_id = std::numeric_limits<goggles::fc::FilterControlId>::max(), .value = 1.0F});

    REQUIRE(controller.reload_shader_preset({}, build_config.device_info, build_config.chain_config)
                .has_value());
    wait_for_reload_start(controller);
    controller.check_pending_chain_swap([] {});
    REQUIRE(controller.consume_chain_swapped());

    const auto prechain_controls =
        controller.list_filter_controls(goggles::fc::FilterControlStage::prechain);
    const auto* filter_type = find_filter_control(prechain_controls, "filter_type");
    REQUIRE(filter_type != nullptr);
    CHECK(filter_type->current_value == Catch::Approx(2.0F));

    goggles::get_logger()->flush();
    auto log_text = read_text_file(log_path);
    REQUIRE(log_text.has_value());
    CHECK(log_text->find("Failed to restore filter control before swap") == std::string::npos);
    CHECK(log_text->find("Failed to restore filter control value before swap") ==
          std::string::npos);
    CHECK(log_text->find("Control id not found on active chain") == std::string::npos);

    REQUIRE(goggles::set_log_file_path({}).has_value());
    controller.shutdown([&fixture]() { vkDeviceWaitIdle(fixture.device_info().device); });
}

TEST_CASE("Controller and backend retarget path stays distinct from reload",
          "[filter_chain][retarget_contract]") {
    const auto controller_cpp = std::filesystem::path(GOGGLES_SOURCE_DIR) /
                                "src/render/backend/filter_chain_controller.cpp";
    const auto backend_cpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/vulkan_backend.cpp";

    auto controller_text = read_text_file(controller_cpp);
    auto backend_text = read_text_file(backend_cpp);
    REQUIRE(controller_text.has_value());
    REQUIRE(backend_text.has_value());

    REQUIRE(controller_text->find("authoritative_output_target = OutputTarget{") !=
            std::string::npos);
    REQUIRE(controller_text->find("align_adapter_output(") != std::string::npos);
    REQUIRE(controller_text->find("new_adapter, requested_output_target,") != std::string::npos);
    REQUIRE(controller_text->find("pending_slot, authoritative_output_target,") !=
            std::string::npos);

    const auto recreate_swapchain_pos =
        backend_text->find("auto VulkanBackend::recreate_swapchain(");
    const auto retarget_call_pos = backend_text->find(
        "m_filter_chain_controller.retarget_filter_chain(", recreate_swapchain_pos);
    const auto reload_call_pos =
        backend_text->find("m_filter_chain_controller.reload_shader_preset(");
    REQUIRE(recreate_swapchain_pos != std::string::npos);
    REQUIRE(retarget_call_pos != std::string::npos);
    REQUIRE(reload_call_pos != std::string::npos);
    REQUIRE(recreate_swapchain_pos < retarget_call_pos);
}

TEST_CASE("Structural live updates rebuild the active adapter contract",
          "[filter_chain][retarget_contract]") {
    const auto controller_cpp = std::filesystem::path(GOGGLES_SOURCE_DIR) /
                                "src/render/backend/filter_chain_controller.cpp";

    auto controller_text = read_text_file(controller_cpp);
    REQUIRE(controller_text.has_value());

    // set_prechain_resolution must call slot.chain.set_prechain_resolution (not resize)
    const auto prechain_update_pos =
        controller_text->find("FilterChainController::set_prechain_resolution(");
    const auto prechain_update_end =
        controller_text->find("FilterChainController::handle_resize(", prechain_update_pos);
    REQUIRE(prechain_update_pos != std::string::npos);
    REQUIRE(prechain_update_end != std::string::npos);
    REQUIRE(controller_text->find("active_slot.chain.set_prechain_resolution(&fc_resolution)",
                                  prechain_update_pos) != std::string::npos);
    // resize must NOT appear inside set_prechain_resolution (it belongs in handle_resize)
    const auto resize_call_pos =
        controller_text->find("active_slot.chain.resize(&extent)", prechain_update_pos);
    REQUIRE((resize_call_pos == std::string::npos || resize_call_pos >= prechain_update_end));

    // reload_shader_preset must propagate stage mask and prechain dimensions into the build config
    REQUIRE(controller_text->find(
                "chain_config.initial_stage_mask =",
                controller_text->find("auto FilterChainController::reload_shader_preset(")) !=
            std::string::npos);
    REQUIRE(controller_text->find(
                "chain_config.initial_prechain_width = source_resolution.width;",
                controller_text->find("auto FilterChainController::reload_shader_preset(")) !=
            std::string::npos);
}
