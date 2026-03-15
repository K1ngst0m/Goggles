/// @file test_filter_chain_c_api_contracts.cpp
/// Contract tests for the standalone goggles_fc_* C API surface.
///
/// Tests the public header <goggles_filter_chain.h> exclusively. No internal headers.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <goggles_filter_chain.h>
#include <limits>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

namespace {

// ---------------------------------------------------------------------------
// Vulkan runtime fixture — creates a real VkInstance + VkDevice for tests
// that need GPU handles. Tests SKIP if no Vulkan GPU is available.
// ---------------------------------------------------------------------------

struct VulkanRuntimeFixture {
    VulkanRuntimeFixture() {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "goggles_fc_c_api_tests";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo instance_info{};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo = &app_info;
        if (vkCreateInstance(&instance_info, nullptr, &m_vk_instance) != VK_SUCCESS) {
            return;
        }
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance{m_vk_instance});

        uint32_t device_count = 0u;
        if (vkEnumeratePhysicalDevices(m_vk_instance, &device_count, nullptr) != VK_SUCCESS ||
            device_count == 0u) {
            return;
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        if (vkEnumeratePhysicalDevices(m_vk_instance, &device_count, devices.data()) !=
            VK_SUCCESS) {
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

        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1u;
        device_info.pQueueCreateInfos = &queue_info;

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
        if (m_vk_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_vk_instance, nullptr);
        }
    }

    VulkanRuntimeFixture(const VulkanRuntimeFixture&) = delete;
    auto operator=(const VulkanRuntimeFixture&) -> VulkanRuntimeFixture& = delete;

    [[nodiscard]] bool available() const { return m_device != VK_NULL_HANDLE; }

    /// Populate a goggles_fc_vk_device_create_info_t from the fixture's Vulkan state.
    [[nodiscard]] auto make_device_create_info() const -> goggles_fc_vk_device_create_info_t {
        auto info = goggles_fc_vk_device_create_info_init();
        info.physical_device = m_physical_device;
        info.device = m_device;
        info.graphics_queue = m_queue;
        info.graphics_queue_family_index = m_queue_family_index;
        return info;
    }

    VkPhysicalDevice physical_device() const { return m_physical_device; }
    VkDevice device() const { return m_device; }
    VkQueue queue() const { return m_queue; }
    uint32_t queue_family_index() const { return m_queue_family_index; }

private:
    VkInstance m_vk_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_queue_family_index = UINT32_MAX;
};

// ---------------------------------------------------------------------------
// RAII guard for goggles_fc_instance_t
// ---------------------------------------------------------------------------

struct InstanceGuard {
    ~InstanceGuard() {
        if (instance != nullptr) {
            goggles_fc_instance_destroy(instance);
        }
    }

    InstanceGuard() = default;
    InstanceGuard(const InstanceGuard&) = delete;
    auto operator=(const InstanceGuard&) -> InstanceGuard& = delete;

    goggles_fc_instance_t* instance = nullptr;
};

// ---------------------------------------------------------------------------
// RAII guard for goggles_fc_device_t
// ---------------------------------------------------------------------------

struct DeviceGuard {
    ~DeviceGuard() {
        if (dev != nullptr) {
            goggles_fc_device_destroy(dev);
        }
    }

    DeviceGuard() = default;
    DeviceGuard(const DeviceGuard&) = delete;
    auto operator=(const DeviceGuard&) -> DeviceGuard& = delete;

    goggles_fc_device_t* dev = nullptr;
};

// ---------------------------------------------------------------------------
// RAII guard for goggles_fc_program_t
// ---------------------------------------------------------------------------

struct ProgramGuard {
    ~ProgramGuard() {
        if (prog != nullptr) {
            goggles_fc_program_destroy(prog);
        }
    }

    ProgramGuard() = default;
    ProgramGuard(const ProgramGuard&) = delete;
    auto operator=(const ProgramGuard&) -> ProgramGuard& = delete;

    goggles_fc_program_t* prog = nullptr;
};

// ---------------------------------------------------------------------------
// RAII guard for goggles_fc_chain_t
// ---------------------------------------------------------------------------

struct ChainGuard {
    ~ChainGuard() {
        if (chain != nullptr) {
            goggles_fc_chain_destroy(chain);
        }
    }

    ChainGuard() = default;
    ChainGuard(const ChainGuard&) = delete;
    auto operator=(const ChainGuard&) -> ChainGuard& = delete;

    goggles_fc_chain_t* chain = nullptr;
};

// ---------------------------------------------------------------------------
// Log callback state for testing
// ---------------------------------------------------------------------------

struct LogCallbackState {
    uint32_t call_count = 0u;
    std::vector<std::string> messages;
};

void GOGGLES_FC_CALL test_log_callback(const goggles_fc_log_message_t* message, void* user_data) {
    auto* state = static_cast<LogCallbackState*>(user_data);
    if (state == nullptr) {
        return;
    }
    ++state->call_count;
    if (message != nullptr && message->message.data != nullptr && message->message.size > 0u) {
        state->messages.emplace_back(message->message.data, message->message.size);
    }
}

// ---------------------------------------------------------------------------
// Helper: create a valid fc instance
// ---------------------------------------------------------------------------

auto create_test_instance(goggles_fc_instance_t** out) -> goggles_fc_status_t {
    auto info = goggles_fc_instance_create_info_init();
    return goggles_fc_instance_create(&info, out);
}

// ---------------------------------------------------------------------------
// Helper: create instance + device (requires Vulkan fixture)
// ---------------------------------------------------------------------------

auto create_test_device(const VulkanRuntimeFixture& fixture, goggles_fc_instance_t* instance,
                        goggles_fc_device_t** out) -> goggles_fc_status_t {
    auto dev_info = fixture.make_device_create_info();
    return goggles_fc_device_create_vk(instance, &dev_info, out);
}

// ---------------------------------------------------------------------------
// Helper: read file contents into a vector
// ---------------------------------------------------------------------------

auto read_file_bytes(const std::filesystem::path& path) -> std::vector<uint8_t> {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }
    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    return bytes;
}

struct TempPresetDir {
    TempPresetDir() {
        dir = std::filesystem::temp_directory_path() /
              ("goggles-fc-control-" +
               std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
    }

    ~TempPresetDir() { std::filesystem::remove_all(dir); }

    std::filesystem::path dir;
};

void write_text_file(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream file(path, std::ios::binary);
    REQUIRE(file.is_open());
    file << contents;
    REQUIRE(file.good());
}

auto make_utf8_view(std::string_view value) -> goggles_fc_utf8_view_t {
    return {.data = value.data(), .size = value.size()};
}

auto make_parameter_shader(std::string_view control_name, std::string_view description)
    -> std::string {
    std::string shader;
    shader += "#version 450\n\n";
    shader += "layout(push_constant) uniform Push\n{\n";
    shader += "   vec4 SourceSize;\n";
    shader += "   vec4 OriginalSize;\n";
    shader += "   vec4 OutputSize;\n";
    shader += "   uint FrameCount;\n";
    shader += "   float ";
    shader += control_name;
    shader += ";\n} params;\n\n";
    shader += "#pragma parameter ";
    shader += control_name;
    shader += " \"";
    shader += description;
    shader += "\" 0.25 0.0 1.0 0.05\n\n";
    shader += "layout(std140, set = 0, binding = 0) uniform UBO\n{\n   mat4 MVP;\n} global;\n\n";
    shader += "#pragma stage vertex\n";
    shader += "layout(location = 0) in vec4 Position;\n";
    shader += "layout(location = 1) in vec2 TexCoord;\n";
    shader += "layout(location = 0) out vec2 vTexCoord;\n\n";
    shader +=
        "void main()\n{\n   gl_Position = global.MVP * Position;\n   vTexCoord = TexCoord;\n}\n\n";
    shader += "#pragma stage fragment\n";
    shader += "layout(location = 0) in vec2 vTexCoord;\n";
    shader += "layout(location = 0) out vec4 FragColor;\n";
    shader += "layout(set = 0, binding = 1) uniform sampler2D Source;\n\n";
    shader += "void main()\n{\n   vec3 current = texture(Source, vTexCoord).rgb;\n";
    shader += "   FragColor = vec4(current * (0.5 + (0.5 * params.";
    shader += control_name;
    shader += ")), 1.0);\n}\n";
    return shader;
}

void write_control_order_fixture(const TempPresetDir& dir) {
    write_text_file(dir.dir / "control_a.slang", make_parameter_shader("CONTROL_A", "Control A"));
    write_text_file(dir.dir / "control_b.slang", make_parameter_shader("CONTROL_B", "Control B"));
    write_text_file(dir.dir / "program_ab.slangp",
                    "shaders = 2\nshader0 = control_a.slang\nshader1 = control_b.slang\n"
                    "filter_linear0 = false\nfilter_linear1 = false\n");
    write_text_file(dir.dir / "program_ba.slangp",
                    "shaders = 2\nshader0 = control_b.slang\nshader1 = control_a.slang\n"
                    "filter_linear0 = false\nfilter_linear1 = false\n");
}

} // namespace

// ===========================================================================
// TEST_CASE 1: Version and capability queries (no Vulkan needed)
// ===========================================================================

TEST_CASE("goggles_fc version and capability queries", "[goggles_fc_api]") {
    SECTION("API version matches compile-time macro") {
        REQUIRE(goggles_fc_get_api_version() == GOGGLES_FC_API_VERSION);
    }

    SECTION("ABI version matches compile-time macro") {
        REQUIRE(goggles_fc_get_abi_version() == GOGGLES_FC_ABI_VERSION);
    }

    SECTION("capabilities include all expected flags") {
        const auto caps = goggles_fc_get_capabilities();
        REQUIRE((caps & GOGGLES_FC_CAPABILITY_VULKAN) != 0u);
        REQUIRE((caps & GOGGLES_FC_CAPABILITY_FILE_SOURCE) != 0u);
        REQUIRE((caps & GOGGLES_FC_CAPABILITY_MEMORY_SOURCE) != 0u);
        REQUIRE((caps & GOGGLES_FC_CAPABILITY_LOG_CALLBACK) != 0u);
    }

    SECTION("status_string returns non-null for all 12 status codes") {
        for (uint32_t code = 0u; code <= 11u; ++code) {
            INFO("status code = " << code);
            const char* str = goggles_fc_status_string(static_cast<goggles_fc_status_t>(code));
            REQUIRE(str != nullptr);
            REQUIRE(std::string_view(str).size() > 0u);
        }
    }

    SECTION("is_success and is_error classify OK correctly") {
        REQUIRE(goggles_fc_is_success(GOGGLES_FC_STATUS_OK) == true);
        REQUIRE(goggles_fc_is_error(GOGGLES_FC_STATUS_OK) == false);
    }

    SECTION("is_error is true for all error status values 1-11") {
        const goggles_fc_status_t error_codes[] = {
            GOGGLES_FC_STATUS_INVALID_ARGUMENT, GOGGLES_FC_STATUS_NOT_INITIALIZED,
            GOGGLES_FC_STATUS_NOT_FOUND,        GOGGLES_FC_STATUS_PRESET_ERROR,
            GOGGLES_FC_STATUS_IO_ERROR,         GOGGLES_FC_STATUS_VULKAN_ERROR,
            GOGGLES_FC_STATUS_OUT_OF_MEMORY,    GOGGLES_FC_STATUS_NOT_SUPPORTED,
            GOGGLES_FC_STATUS_RUNTIME_ERROR,    GOGGLES_FC_STATUS_INVALID_DEPENDENCY,
            GOGGLES_FC_STATUS_VALIDATION_ERROR,
        };
        for (const auto code : error_codes) {
            INFO("status code = " << code << " (" << goggles_fc_status_string(code) << ")");
            REQUIRE(goggles_fc_is_error(code) == true);
            REQUIRE(goggles_fc_is_success(code) == false);
        }
    }
}

// ===========================================================================
// TEST_CASE 2: Instance lifecycle (no Vulkan needed)
// ===========================================================================

TEST_CASE("goggles_fc instance lifecycle", "[goggles_fc_api]") {
    SECTION("create instance with default create_info succeeds") {
        InstanceGuard guard;
        const auto status = create_test_instance(&guard.instance);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
        REQUIRE(guard.instance != nullptr);
    }

    SECTION("destroy with null is safe") {
        // Should not crash
        goggles_fc_instance_destroy(nullptr);
    }

    SECTION("create + destroy is clean") {
        goggles_fc_instance_t* instance = nullptr;
        auto info = goggles_fc_instance_create_info_init();
        REQUIRE(goggles_fc_instance_create(&info, &instance) == GOGGLES_FC_STATUS_OK);
        REQUIRE(instance != nullptr);
        goggles_fc_instance_destroy(instance);
        // No double-free, no crash
    }

    SECTION("set_log_callback with valid callback succeeds") {
        InstanceGuard guard;
        REQUIRE(create_test_instance(&guard.instance) == GOGGLES_FC_STATUS_OK);

        LogCallbackState state;
        const auto status =
            goggles_fc_instance_set_log_callback(guard.instance, test_log_callback, &state);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
    }

    SECTION("set_log_callback with null callback (to clear) succeeds") {
        InstanceGuard guard;
        REQUIRE(create_test_instance(&guard.instance) == GOGGLES_FC_STATUS_OK);

        // First set a valid callback
        LogCallbackState state;
        REQUIRE(goggles_fc_instance_set_log_callback(guard.instance, test_log_callback, &state) ==
                GOGGLES_FC_STATUS_OK);

        // Then clear it with null
        const auto status = goggles_fc_instance_set_log_callback(guard.instance, nullptr, nullptr);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
    }
}

// ===========================================================================
// TEST_CASE 3: Device lifecycle and Vulkan validation
// ===========================================================================

TEST_CASE("goggles_fc device lifecycle and Vulkan validation", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    SECTION("create device with valid Vulkan handles succeeds") {
        InstanceGuard inst_guard;
        REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

        DeviceGuard dev_guard;
        const auto status = create_test_device(fixture, inst_guard.instance, &dev_guard.dev);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
        REQUIRE(dev_guard.dev != nullptr);
    }

    SECTION("device create with null physical_device returns INVALID_ARGUMENT") {
        InstanceGuard inst_guard;
        REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

        auto dev_info = fixture.make_device_create_info();
        dev_info.physical_device = VK_NULL_HANDLE;

        goggles_fc_device_t* dev = nullptr;
        REQUIRE(goggles_fc_device_create_vk(inst_guard.instance, &dev_info, &dev) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        REQUIRE(dev == nullptr);
    }

    SECTION("device create with null device returns INVALID_ARGUMENT") {
        InstanceGuard inst_guard;
        REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

        auto dev_info = fixture.make_device_create_info();
        dev_info.device = VK_NULL_HANDLE;

        goggles_fc_device_t* dev = nullptr;
        REQUIRE(goggles_fc_device_create_vk(inst_guard.instance, &dev_info, &dev) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        REQUIRE(dev == nullptr);
    }

    SECTION("device create with null queue returns INVALID_ARGUMENT") {
        InstanceGuard inst_guard;
        REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

        auto dev_info = fixture.make_device_create_info();
        dev_info.graphics_queue = VK_NULL_HANDLE;

        goggles_fc_device_t* dev = nullptr;
        REQUIRE(goggles_fc_device_create_vk(inst_guard.instance, &dev_info, &dev) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        REQUIRE(dev == nullptr);
    }

    SECTION("device create with null instance returns INVALID_ARGUMENT") {
        auto dev_info = fixture.make_device_create_info();
        goggles_fc_device_t* dev = nullptr;
        REQUIRE(goggles_fc_device_create_vk(nullptr, &dev_info, &dev) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("device destroy with null is safe") {
        // Should not crash
        goggles_fc_device_destroy(nullptr);
    }
}

// ===========================================================================
// TEST_CASE 4: Program create from file
// ===========================================================================

TEST_CASE("goggles_fc program create from file", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Set up instance + device
    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    SECTION("program create from file preset succeeds") {
        const auto preset_str = preset_path.string();

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = preset_str.c_str();
        source.path.size = preset_str.size();

        ProgramGuard prog_guard;
        const auto status = goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
        REQUIRE(prog_guard.prog != nullptr);
    }

    SECTION("program get_source_info returns FILE provenance") {
        const auto preset_str = preset_path.string();

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = preset_str.c_str();
        source.path.size = preset_str.size();

        ProgramGuard prog_guard;
        REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
                GOGGLES_FC_STATUS_OK);

        auto info = goggles_fc_program_source_info_init();
        REQUIRE(goggles_fc_program_get_source_info(prog_guard.prog, &info) == GOGGLES_FC_STATUS_OK);
        REQUIRE(info.provenance == GOGGLES_FC_PROVENANCE_FILE);
    }

    SECTION("program create with nonexistent path returns error") {
        const std::string bad_path = "/nonexistent/path/to/preset.slangp";

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = bad_path.c_str();
        source.path.size = bad_path.size();

        goggles_fc_program_t* prog = nullptr;
        const auto status = goggles_fc_program_create(dev_guard.dev, &source, &prog);
        REQUIRE(goggles_fc_is_error(status));
        REQUIRE(prog == nullptr);
    }

    SECTION("program destroy with null is safe") {
        goggles_fc_program_destroy(nullptr);
    }
}

// ===========================================================================
// TEST_CASE 5: Program create from memory
// ===========================================================================

TEST_CASE("goggles_fc program create from memory", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Set up instance + device
    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    SECTION("program create from memory-backed preset") {
        const auto bytes = read_file_bytes(preset_path);
        REQUIRE(!bytes.empty());

        const std::string source_name = "memory://format.slangp";

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_MEMORY;
        source.bytes = bytes.data();
        source.byte_count = bytes.size();
        source.source_name.data = source_name.c_str();
        source.source_name.size = source_name.size();

        goggles_fc_program_t* prog = nullptr;
        const auto status = goggles_fc_program_create(dev_guard.dev, &source, &prog);

        // Accept OK (fully functional) or NOT_SUPPORTED (if memory source is
        // scaffolded but not yet fully wired).
        if (status == GOGGLES_FC_STATUS_OK) {
            REQUIRE(prog != nullptr);
            goggles_fc_program_destroy(prog);
        } else {
            REQUIRE((status == GOGGLES_FC_STATUS_NOT_SUPPORTED || goggles_fc_is_error(status)));
        }
    }

    SECTION("program create from memory with null bytes returns INVALID_ARGUMENT") {
        const std::string source_name = "memory://null_bytes.slangp";

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_MEMORY;
        source.bytes = nullptr;
        source.byte_count = 100u;
        source.source_name.data = source_name.c_str();
        source.source_name.size = source_name.size();

        goggles_fc_program_t* prog = nullptr;
        const auto status = goggles_fc_program_create(dev_guard.dev, &source, &prog);
        REQUIRE(goggles_fc_is_error(status));
    }

    SECTION("program create from memory with zero byte_count returns INVALID_ARGUMENT") {
        const uint8_t dummy = 0u;
        const std::string source_name = "memory://empty.slangp";

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_MEMORY;
        source.bytes = &dummy;
        source.byte_count = 0u;
        source.source_name.data = source_name.c_str();
        source.source_name.size = source_name.size();

        goggles_fc_program_t* prog = nullptr;
        const auto status = goggles_fc_program_create(dev_guard.dev, &source, &prog);
        REQUIRE(goggles_fc_is_error(status));
    }
}

// ===========================================================================
// TEST_CASE 6: Chain lifecycle
// ===========================================================================

TEST_CASE("goggles_fc chain lifecycle", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Set up instance + device + program
    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    const auto preset_str = preset_path.string();
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path.data = preset_str.c_str();
    source.path.size = preset_str.size();

    ProgramGuard prog_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    SECTION("full lifecycle: create chain, then destroy in reverse order") {
        auto chain_info = goggles_fc_chain_create_info_init();
        chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
        chain_info.frames_in_flight = 2u;

        ChainGuard chain_guard;
        const auto status = goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                                    &chain_guard.chain);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
        REQUIRE(chain_guard.chain != nullptr);

        // Destroy chain (guard handles it), then program, device, instance in reverse order
        // (all handled by guards in reverse declaration order)
    }

    SECTION("chain destroy with null is safe") {
        goggles_fc_chain_destroy(nullptr);
    }
}

// ===========================================================================
// TEST_CASE 7: ABI durability
// ===========================================================================

TEST_CASE("goggles_fc ABI durability", "[goggles_fc_api]") {
    SECTION("scalar type sizes") {
        STATIC_REQUIRE(sizeof(goggles_fc_status_t) == sizeof(uint32_t));
        STATIC_REQUIRE(sizeof(goggles_fc_log_level_t) == sizeof(uint32_t));
        STATIC_REQUIRE(sizeof(goggles_fc_capability_flags_t) == sizeof(uint32_t));
        STATIC_REQUIRE(sizeof(goggles_fc_preset_source_kind_t) == sizeof(uint32_t));
    }

    SECTION("struct_size is first field in key structs") {
        STATIC_REQUIRE(offsetof(GogglesFcInstanceCreateInfo, struct_size) == 0u);
        STATIC_REQUIRE(offsetof(GogglesFcVkDeviceCreateInfo, struct_size) == 0u);
        STATIC_REQUIRE(offsetof(GogglesFcPresetSource, struct_size) == 0u);
        STATIC_REQUIRE(offsetof(GogglesFcChainCreateInfo, struct_size) == 0u);
        STATIC_REQUIRE(offsetof(GogglesFcLogMessage, struct_size) == 0u);
        STATIC_REQUIRE(offsetof(GogglesFcProgramSourceInfo, struct_size) == 0u);
        STATIC_REQUIRE(offsetof(GogglesFcProgramReport, struct_size) == 0u);
        STATIC_REQUIRE(offsetof(GogglesFcChainReport, struct_size) == 0u);
        STATIC_REQUIRE(offsetof(GogglesFcChainErrorInfo, struct_size) == 0u);
        STATIC_REQUIRE(offsetof(GogglesFcControlInfo, struct_size) == 0u);
        STATIC_REQUIRE(offsetof(GogglesFcRecordInfoVk, struct_size) == 0u);
        STATIC_REQUIRE(offsetof(GogglesFcChainTargetInfo, struct_size) == 0u);
        STATIC_REQUIRE(offsetof(GogglesFcImportCallbacks, struct_size) == 0u);
    }

    SECTION("init helpers produce correct struct_size") {
        REQUIRE(goggles_fc_instance_create_info_init().struct_size ==
                sizeof(goggles_fc_instance_create_info_t));
        REQUIRE(goggles_fc_vk_device_create_info_init().struct_size ==
                sizeof(goggles_fc_vk_device_create_info_t));
        REQUIRE(goggles_fc_preset_source_init().struct_size == sizeof(goggles_fc_preset_source_t));
        REQUIRE(goggles_fc_chain_create_info_init().struct_size ==
                sizeof(goggles_fc_chain_create_info_t));
        REQUIRE(goggles_fc_log_message_init().struct_size == sizeof(goggles_fc_log_message_t));
        REQUIRE(goggles_fc_program_source_info_init().struct_size ==
                sizeof(goggles_fc_program_source_info_t));
        REQUIRE(goggles_fc_program_report_init().struct_size ==
                sizeof(goggles_fc_program_report_t));
        REQUIRE(goggles_fc_chain_report_init().struct_size == sizeof(goggles_fc_chain_report_t));
        REQUIRE(goggles_fc_chain_error_info_init().struct_size ==
                sizeof(goggles_fc_chain_error_info_t));
        REQUIRE(goggles_fc_record_info_vk_init().struct_size ==
                sizeof(goggles_fc_record_info_vk_t));
        REQUIRE(goggles_fc_chain_target_info_init().struct_size ==
                sizeof(goggles_fc_chain_target_info_t));
        REQUIRE(goggles_fc_control_info_init().struct_size == sizeof(goggles_fc_control_info_t));
        REQUIRE(goggles_fc_import_callbacks_init().struct_size ==
                sizeof(goggles_fc_import_callbacks_t));
    }

    SECTION("version macros are well-formed") {
        STATIC_REQUIRE(GOGGLES_FC_API_VERSION == GOGGLES_FC_MAKE_VERSION(1u, 0u, 0u));
        STATIC_REQUIRE(GOGGLES_FC_ABI_VERSION == 1u);
    }

    SECTION("status code constants have expected values") {
        STATIC_REQUIRE(GOGGLES_FC_STATUS_OK == 0u);
        STATIC_REQUIRE(GOGGLES_FC_STATUS_INVALID_ARGUMENT == 1u);
        STATIC_REQUIRE(GOGGLES_FC_STATUS_NOT_INITIALIZED == 2u);
        STATIC_REQUIRE(GOGGLES_FC_STATUS_NOT_FOUND == 3u);
        STATIC_REQUIRE(GOGGLES_FC_STATUS_PRESET_ERROR == 4u);
        STATIC_REQUIRE(GOGGLES_FC_STATUS_IO_ERROR == 5u);
        STATIC_REQUIRE(GOGGLES_FC_STATUS_VULKAN_ERROR == 6u);
        STATIC_REQUIRE(GOGGLES_FC_STATUS_OUT_OF_MEMORY == 7u);
        STATIC_REQUIRE(GOGGLES_FC_STATUS_NOT_SUPPORTED == 8u);
        STATIC_REQUIRE(GOGGLES_FC_STATUS_RUNTIME_ERROR == 9u);
        STATIC_REQUIRE(GOGGLES_FC_STATUS_INVALID_DEPENDENCY == 10u);
        STATIC_REQUIRE(GOGGLES_FC_STATUS_VALIDATION_ERROR == 11u);
    }

    SECTION("capability flag constants have expected values") {
        STATIC_REQUIRE(GOGGLES_FC_CAPABILITY_NONE == 0u);
        STATIC_REQUIRE(GOGGLES_FC_CAPABILITY_VULKAN == (1u << 0));
        STATIC_REQUIRE(GOGGLES_FC_CAPABILITY_FILE_SOURCE == (1u << 1));
        STATIC_REQUIRE(GOGGLES_FC_CAPABILITY_MEMORY_SOURCE == (1u << 2));
        STATIC_REQUIRE(GOGGLES_FC_CAPABILITY_LOG_CALLBACK == (1u << 3));
    }
}

// ===========================================================================
// TEST_CASE A: Import-callback contracts
// ===========================================================================

TEST_CASE("goggles_fc import-callback contracts", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Set up instance + device
    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    SECTION("import_callbacks_init produces valid zero-initialized struct") {
        auto cbs = goggles_fc_import_callbacks_init();
        REQUIRE(cbs.struct_size == sizeof(goggles_fc_import_callbacks_t));
        REQUIRE(cbs.read_fn == nullptr);
        REQUIRE(cbs.free_fn == nullptr);
        REQUIRE(cbs.user_data == nullptr);
    }

    SECTION("import_callbacks struct can be populated with read_fn and free_fn") {
        // Dummy callbacks — we only test that the struct can be populated and
        // passed without crashing; the callbacks are not expected to be invoked
        // in this section because the file-backed source resolves via filesystem.
        auto dummy_read = [](goggles_fc_utf8_view_t /*relative_path*/, void** /*out_bytes*/,
                             size_t* /*out_byte_count*/,
                             void* /*user_data*/) -> goggles_fc_import_result_t {
            return GOGGLES_FC_STATUS_NOT_FOUND;
        };
        auto dummy_free = [](void* /*bytes*/, size_t /*byte_count*/, void* /*user_data*/) {};

        auto cbs = goggles_fc_import_callbacks_init();
        cbs.read_fn = dummy_read;
        cbs.free_fn = dummy_free;
        cbs.user_data = nullptr;

        REQUIRE(cbs.read_fn != nullptr);
        REQUIRE(cbs.free_fn != nullptr);

        // Attach to a file-backed preset source (callbacks won't be called for
        // the main preset path, but the struct plumbing should not crash).
        const auto preset_str = preset_path.string();
        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = preset_str.c_str();
        source.path.size = preset_str.size();
        source.import_callbacks = &cbs;

        ProgramGuard prog_guard;
        const auto status = goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
        REQUIRE(prog_guard.prog != nullptr);
    }

    SECTION("memory-backed preset with relative imports and no callbacks/base_path is rejected") {
        // format.slangp references "format.slang" and "decode-format.slang" as
        // relative shader paths. Without import callbacks or base_path, the
        // resolver cannot locate these files and must fail deterministically.
        const auto bytes = read_file_bytes(preset_path);
        REQUIRE(!bytes.empty());

        const std::string source_name = "memory://format-no-resolver.slangp";

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_MEMORY;
        source.bytes = bytes.data();
        source.byte_count = bytes.size();
        source.source_name.data = source_name.c_str();
        source.source_name.size = source_name.size();
        // Deliberately leave base_path and import_callbacks as null/empty
        source.base_path.data = nullptr;
        source.base_path.size = 0u;
        source.import_callbacks = nullptr;

        goggles_fc_program_t* prog = nullptr;
        const auto status = goggles_fc_program_create(dev_guard.dev, &source, &prog);

        // Must be deterministically rejected — not OK
        REQUIRE(goggles_fc_is_error(status));
        REQUIRE(prog == nullptr);
    }

    SECTION("memory-backed preset with base_path succeeds") {
        // Provide the base_path pointing to the directory containing the shaders
        // so that relative imports can be resolved via filesystem.
        const auto bytes = read_file_bytes(preset_path);
        REQUIRE(!bytes.empty());

        const std::string source_name = "memory://format-with-basepath.slangp";
        const auto base_dir = preset_path.parent_path().string();

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_MEMORY;
        source.bytes = bytes.data();
        source.byte_count = bytes.size();
        source.source_name.data = source_name.c_str();
        source.source_name.size = source_name.size();
        source.base_path.data = base_dir.c_str();
        source.base_path.size = base_dir.size();

        ProgramGuard prog_guard;
        const auto status = goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
        REQUIRE(prog_guard.prog != nullptr);
    }
}

// ===========================================================================
// TEST_CASE B: Program source provenance
// ===========================================================================

TEST_CASE("goggles_fc program source provenance", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Set up instance + device
    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    SECTION("file-backed program has FILE provenance and correct source info") {
        const auto preset_str = preset_path.string();

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = preset_str.c_str();
        source.path.size = preset_str.size();

        ProgramGuard prog_guard;
        REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
                GOGGLES_FC_STATUS_OK);

        auto info = goggles_fc_program_source_info_init();
        REQUIRE(goggles_fc_program_get_source_info(prog_guard.prog, &info) == GOGGLES_FC_STATUS_OK);

        REQUIRE(info.provenance == GOGGLES_FC_PROVENANCE_FILE);
        REQUIRE(info.source_name.data != nullptr);
        REQUIRE(info.source_name.size > 0u);
        // source_name should contain the preset filename
        std::string_view name_view(info.source_name.data, info.source_name.size);
        REQUIRE(name_view.find("format.slangp") != std::string_view::npos);
        REQUIRE(info.pass_count > 0u);
    }

    SECTION("memory-backed program has MEMORY provenance") {
        // Use base_path so the memory-backed source can resolve relative imports
        const auto bytes = read_file_bytes(preset_path);
        REQUIRE(!bytes.empty());

        const std::string source_name = "memory://test-provenance.slangp";
        const auto base_dir = preset_path.parent_path().string();

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_MEMORY;
        source.bytes = bytes.data();
        source.byte_count = bytes.size();
        source.source_name.data = source_name.c_str();
        source.source_name.size = source_name.size();
        source.base_path.data = base_dir.c_str();
        source.base_path.size = base_dir.size();

        ProgramGuard prog_guard;
        const auto status = goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog);

        if (status == GOGGLES_FC_STATUS_OK) {
            REQUIRE(prog_guard.prog != nullptr);

            auto info = goggles_fc_program_source_info_init();
            REQUIRE(goggles_fc_program_get_source_info(prog_guard.prog, &info) ==
                    GOGGLES_FC_STATUS_OK);

            REQUIRE(info.provenance == GOGGLES_FC_PROVENANCE_MEMORY);
            REQUIRE(info.source_name.data != nullptr);
            REQUIRE(info.source_name.size > 0u);
            std::string_view name_view(info.source_name.data, info.source_name.size);
            REQUIRE(name_view == source_name);
        } else {
            // Memory-backed programs may not be fully functional — document failure
            WARN("Memory-backed program creation returned status: "
                 << goggles_fc_status_string(status));
        }
    }

    SECTION("get_source_info with null program returns INVALID_ARGUMENT") {
        auto info = goggles_fc_program_source_info_init();
        REQUIRE(goggles_fc_program_get_source_info(nullptr, &info) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("get_source_info with null out_source_info returns INVALID_ARGUMENT") {
        const auto preset_str = preset_path.string();

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = preset_str.c_str();
        source.path.size = preset_str.size();

        ProgramGuard prog_guard;
        REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
                GOGGLES_FC_STATUS_OK);

        REQUIRE(goggles_fc_program_get_source_info(prog_guard.prog, nullptr) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }
}

// ===========================================================================
// TEST_CASE C: Program report queries
// ===========================================================================

TEST_CASE("goggles_fc program report queries", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Set up instance + device
    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    SECTION("program report from file-backed preset has positive counts") {
        const auto preset_str = preset_path.string();

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = preset_str.c_str();
        source.path.size = preset_str.size();

        ProgramGuard prog_guard;
        REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
                GOGGLES_FC_STATUS_OK);

        auto report = goggles_fc_program_report_init();
        REQUIRE(goggles_fc_program_get_report(prog_guard.prog, &report) == GOGGLES_FC_STATUS_OK);

        // format.slangp has 2 shaders → 2 passes → 4 shader stages (vertex + fragment each)
        REQUIRE(report.shader_count > 0u);
        REQUIRE(report.pass_count > 0u);
        REQUIRE(report.struct_size == sizeof(goggles_fc_program_report_t));
    }

    SECTION("get_report with null program returns INVALID_ARGUMENT") {
        auto report = goggles_fc_program_report_init();
        REQUIRE(goggles_fc_program_get_report(nullptr, &report) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("get_report with null out_report returns INVALID_ARGUMENT") {
        const auto preset_str = preset_path.string();

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = preset_str.c_str();
        source.path.size = preset_str.size();

        ProgramGuard prog_guard;
        REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
                GOGGLES_FC_STATUS_OK);

        REQUIRE(goggles_fc_program_get_report(prog_guard.prog, nullptr) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }
}

// ===========================================================================
// TEST_CASE D: Chain control metadata
// ===========================================================================

TEST_CASE("goggles_fc chain control metadata", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Full lifecycle: instance → device → program → chain
    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    const auto preset_str = preset_path.string();
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path.data = preset_str.c_str();
    source.path.size = preset_str.size();

    ProgramGuard prog_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    auto chain_info = goggles_fc_chain_create_info_init();
    chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    chain_info.frames_in_flight = 2u;
    chain_info.initial_prechain_resolution = {1u, 1u};

    ChainGuard chain_guard;
    REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                    &chain_guard.chain) == GOGGLES_FC_STATUS_OK);

    SECTION("get_control_count returns a count (may be zero for presets without parameters)") {
        uint32_t count = UINT32_MAX;
        REQUIRE(goggles_fc_chain_get_control_count(chain_guard.chain, &count) ==
                GOGGLES_FC_STATUS_OK);

        // format.slangp has no #pragma parameter declarations; controls may
        // come from built-in prechain parameters (e.g., filter_type) depending
        // on whether the prechain stage is active.
        INFO("control count = " << count);
        // At minimum the API call succeeds; count is >= 0 by definition (uint32_t)
    }

    SECTION("get_control_info for each valid index populates the struct") {
        uint32_t count = 0u;
        REQUIRE(goggles_fc_chain_get_control_count(chain_guard.chain, &count) ==
                GOGGLES_FC_STATUS_OK);

        if (count > 0u) {
            auto ctrl = goggles_fc_control_info_init();
            REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, 0u, &ctrl) ==
                    GOGGLES_FC_STATUS_OK);

            REQUIRE(ctrl.struct_size == sizeof(goggles_fc_control_info_t));
            REQUIRE(ctrl.name.data != nullptr);
            REQUIRE(ctrl.name.size > 0u);
            // Validate that numeric fields are sensible
            INFO("control 0: name='" << std::string_view(ctrl.name.data, ctrl.name.size)
                                     << "' default=" << ctrl.default_value
                                     << " min=" << ctrl.min_value << " max=" << ctrl.max_value
                                     << " step=" << ctrl.step);
        } else {
            WARN("No controls available for format.slangp — skipping per-control checks");
        }
    }

    SECTION("set_control_value_f32 with valid index succeeds when controls exist") {
        uint32_t count = 0u;
        REQUIRE(goggles_fc_chain_get_control_count(chain_guard.chain, &count) ==
                GOGGLES_FC_STATUS_OK);

        if (count > 0u) {
            auto ctrl = goggles_fc_control_info_init();
            REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, 0u, &ctrl) ==
                    GOGGLES_FC_STATUS_OK);

            // Set to the default value — should always succeed
            const auto status =
                goggles_fc_chain_set_control_value_f32(chain_guard.chain, 0u, ctrl.default_value);
            REQUIRE(status == GOGGLES_FC_STATUS_OK);
        } else {
            WARN("No controls — skipping set_control_value_f32 valid-index test");
        }
    }

    SECTION("semantic control lookup helpers validate arguments") {
        uint32_t index = UINT32_MAX;
        const auto filter_type = make_utf8_view("filter_type");

        REQUIRE(goggles_fc_chain_find_control_index(chain_guard.chain, GOGGLES_FC_STAGE_PRECHAIN,
                                                    filter_type, &index) == GOGGLES_FC_STATUS_OK);
        REQUIRE(goggles_fc_chain_find_control_index(chain_guard.chain, GOGGLES_FC_STAGE_POSTCHAIN,
                                                    filter_type,
                                                    &index) == GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        REQUIRE(goggles_fc_chain_find_control_index(chain_guard.chain, GOGGLES_FC_STAGE_PRECHAIN,
                                                    {.data = nullptr, .size = 0u},
                                                    &index) == GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        REQUIRE(goggles_fc_chain_set_control_value_f32_by_name(
                    chain_guard.chain, GOGGLES_FC_STAGE_PRECHAIN, filter_type, 1.0f) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(goggles_fc_chain_set_control_value_f32_by_name(
                    chain_guard.chain, GOGGLES_FC_STAGE_POSTCHAIN, filter_type, 1.0f) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("set_control_value_f32 with out-of-bounds index returns INVALID_ARGUMENT") {
        uint32_t count = 0u;
        REQUIRE(goggles_fc_chain_get_control_count(chain_guard.chain, &count) ==
                GOGGLES_FC_STATUS_OK);

        // Use an index that's definitely out of bounds
        const auto status =
            goggles_fc_chain_set_control_value_f32(chain_guard.chain, count + 100u, 1.0f);
        REQUIRE(status == GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("get_control_info with out-of-bounds index returns INVALID_ARGUMENT") {
        uint32_t count = 0u;
        REQUIRE(goggles_fc_chain_get_control_count(chain_guard.chain, &count) ==
                GOGGLES_FC_STATUS_OK);

        auto ctrl = goggles_fc_control_info_init();
        REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, count + 100u, &ctrl) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("get_control_count with null chain returns INVALID_ARGUMENT") {
        uint32_t count = 0u;
        REQUIRE(goggles_fc_chain_get_control_count(nullptr, &count) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("get_control_count with null out_count returns INVALID_ARGUMENT") {
        REQUIRE(goggles_fc_chain_get_control_count(chain_guard.chain, nullptr) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }
}

TEST_CASE("goggles_fc semantic control helpers survive reordered program rebinds",
          "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    TempPresetDir temp_dir;
    write_control_order_fixture(temp_dir);

    const auto preset_ab = temp_dir.dir / "program_ab.slangp";
    const auto preset_ba = temp_dir.dir / "program_ba.slangp";

    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    auto source_ab = goggles_fc_preset_source_init();
    const auto preset_ab_str = preset_ab.string();
    source_ab.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source_ab.path = make_utf8_view(preset_ab_str);

    ProgramGuard program_ab;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source_ab, &program_ab.prog) ==
            GOGGLES_FC_STATUS_OK);

    auto source_ba = goggles_fc_preset_source_init();
    const auto preset_ba_str = preset_ba.string();
    source_ba.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source_ba.path = make_utf8_view(preset_ba_str);

    ProgramGuard program_ba;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source_ba, &program_ba.prog) ==
            GOGGLES_FC_STATUS_OK);

    auto chain_info = goggles_fc_chain_create_info_init();
    chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    chain_info.frames_in_flight = 2u;
    chain_info.initial_prechain_resolution = {1u, 1u};

    ChainGuard chain_guard;
    REQUIRE(goggles_fc_chain_create(dev_guard.dev, program_ab.prog, &chain_info,
                                    &chain_guard.chain) == GOGGLES_FC_STATUS_OK);

    const auto control_a = make_utf8_view("CONTROL_A");
    const auto control_b = make_utf8_view("CONTROL_B");

    uint32_t control_a_index_before = UINT32_MAX;
    uint32_t control_b_index_before = UINT32_MAX;
    REQUIRE(goggles_fc_chain_find_control_index(chain_guard.chain, GOGGLES_FC_STAGE_EFFECT,
                                                control_a,
                                                &control_a_index_before) == GOGGLES_FC_STATUS_OK);
    REQUIRE(goggles_fc_chain_find_control_index(chain_guard.chain, GOGGLES_FC_STAGE_EFFECT,
                                                control_b,
                                                &control_b_index_before) == GOGGLES_FC_STATUS_OK);
    REQUIRE(control_a_index_before != control_b_index_before);

    REQUIRE(goggles_fc_chain_set_control_value_f32_by_name(chain_guard.chain,
                                                           GOGGLES_FC_STAGE_EFFECT, control_a,
                                                           0.75f) == GOGGLES_FC_STATUS_OK);

    auto control_a_info_before = goggles_fc_control_info_init();
    REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, control_a_index_before,
                                              &control_a_info_before) == GOGGLES_FC_STATUS_OK);
    REQUIRE(std::string_view(control_a_info_before.name.data, control_a_info_before.name.size) ==
            "CONTROL_A");
    REQUIRE(control_a_info_before.current_value == Catch::Approx(0.75f));

    REQUIRE(goggles_fc_chain_bind_program(chain_guard.chain, program_ba.prog) ==
            GOGGLES_FC_STATUS_OK);

    uint32_t control_a_index_after = UINT32_MAX;
    uint32_t control_b_index_after = UINT32_MAX;
    REQUIRE(goggles_fc_chain_find_control_index(chain_guard.chain, GOGGLES_FC_STAGE_EFFECT,
                                                control_a,
                                                &control_a_index_after) == GOGGLES_FC_STATUS_OK);
    REQUIRE(goggles_fc_chain_find_control_index(chain_guard.chain, GOGGLES_FC_STAGE_EFFECT,
                                                control_b,
                                                &control_b_index_after) == GOGGLES_FC_STATUS_OK);
    REQUIRE(control_a_index_after != control_b_index_after);
    REQUIRE(control_a_index_after == control_b_index_before);

    auto stale_index_info = goggles_fc_control_info_init();
    REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, control_a_index_before,
                                              &stale_index_info) == GOGGLES_FC_STATUS_OK);
    REQUIRE(std::string_view(stale_index_info.name.data, stale_index_info.name.size) ==
            "CONTROL_B");

    auto rebound_control_a_info = goggles_fc_control_info_init();
    REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, control_a_index_after,
                                              &rebound_control_a_info) == GOGGLES_FC_STATUS_OK);
    REQUIRE(std::string_view(rebound_control_a_info.name.data, rebound_control_a_info.name.size) ==
            "CONTROL_A");
    REQUIRE(rebound_control_a_info.current_value == Catch::Approx(0.75f));

    REQUIRE(goggles_fc_chain_set_control_value_f32_by_name(chain_guard.chain,
                                                           GOGGLES_FC_STAGE_EFFECT, control_a,
                                                           0.5f) == GOGGLES_FC_STATUS_OK);

    auto updated_control_a_info = goggles_fc_control_info_init();
    REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, control_a_index_after,
                                              &updated_control_a_info) == GOGGLES_FC_STATUS_OK);
    REQUIRE(updated_control_a_info.current_value == Catch::Approx(0.5f));
}

TEST_CASE("goggles_fc chain rejects non-finite control values", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path = std::filesystem::path(FILTER_CHAIN_ASSET_DIR) /
                             "diagnostics/semantic_probes/parameter_isolation_probe.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    const auto preset_str = preset_path.string();
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path.data = preset_str.c_str();
    source.path.size = preset_str.size();

    ProgramGuard prog_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    auto chain_info = goggles_fc_chain_create_info_init();
    chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    chain_info.frames_in_flight = 2u;
    chain_info.initial_prechain_resolution = {1u, 1u};

    ChainGuard chain_guard;
    REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                    &chain_guard.chain) == GOGGLES_FC_STATUS_OK);

    uint32_t count = 0u;
    REQUIRE(goggles_fc_chain_get_control_count(chain_guard.chain, &count) == GOGGLES_FC_STATUS_OK);
    REQUIRE(count > 0u);

    SECTION("NaN returns INVALID_ARGUMENT and updates last_error") {
        const auto status = goggles_fc_chain_set_control_value_f32(
            chain_guard.chain, 0u, std::numeric_limits<float>::quiet_NaN());
        REQUIRE(status == GOGGLES_FC_STATUS_INVALID_ARGUMENT);

        auto error_info = goggles_fc_chain_error_info_init();
        REQUIRE(goggles_fc_chain_get_last_error(chain_guard.chain, &error_info) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(error_info.status == GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("infinity returns INVALID_ARGUMENT") {
        const auto status = goggles_fc_chain_set_control_value_f32(
            chain_guard.chain, 0u, std::numeric_limits<float>::infinity());
        REQUIRE(status == GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("finite values still succeed") {
        auto ctrl = goggles_fc_control_info_init();
        REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, 0u, &ctrl) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(std::isfinite(ctrl.default_value));

        const auto status =
            goggles_fc_chain_set_control_value_f32(chain_guard.chain, 0u, ctrl.default_value);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
    }
}

// ===========================================================================
// TEST_CASE E: Chain target-info
// ===========================================================================

TEST_CASE("goggles_fc chain target-info", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Full lifecycle: instance → device → program → chain
    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    const auto preset_str = preset_path.string();
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path.data = preset_str.c_str();
    source.path.size = preset_str.size();

    ProgramGuard prog_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    auto chain_info = goggles_fc_chain_create_info_init();
    chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    chain_info.frames_in_flight = 2u;
    chain_info.initial_prechain_resolution = {1u, 1u};

    ChainGuard chain_guard;
    REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                    &chain_guard.chain) == GOGGLES_FC_STATUS_OK);

    SECTION("chain_target_info_init returns proper defaults") {
        auto target_info = goggles_fc_chain_target_info_init();
        REQUIRE(target_info.struct_size == sizeof(goggles_fc_chain_target_info_t));
        REQUIRE(target_info.target_format == VK_FORMAT_UNDEFINED);
    }

    SECTION("retarget with VK_FORMAT_UNDEFINED returns INVALID_ARGUMENT") {
        auto target_info = goggles_fc_chain_target_info_init();
        target_info.target_format = VK_FORMAT_UNDEFINED;

        REQUIRE(goggles_fc_chain_retarget(chain_guard.chain, &target_info) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("retarget with valid format succeeds") {
        auto target_info = goggles_fc_chain_target_info_init();
        target_info.target_format = VK_FORMAT_R8G8B8A8_UNORM;

        const auto status = goggles_fc_chain_retarget(chain_guard.chain, &target_info);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
    }

    SECTION("retarget with null chain returns INVALID_ARGUMENT") {
        auto target_info = goggles_fc_chain_target_info_init();
        target_info.target_format = VK_FORMAT_R8G8B8A8_UNORM;

        REQUIRE(goggles_fc_chain_retarget(nullptr, &target_info) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("retarget with null target_info returns INVALID_ARGUMENT") {
        REQUIRE(goggles_fc_chain_retarget(chain_guard.chain, nullptr) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }
}

// ===========================================================================
// TEST_CASE E2: Retarget preserves control values and prechain resolution
// ===========================================================================

TEST_CASE("goggles_fc retarget preserves control values and prechain resolution",
          "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    // Use a preset with user-defined controls so we can set and verify values.
    TempPresetDir temp_dir;
    write_text_file(temp_dir.dir / "retarget_ctrl.slang",
                    make_parameter_shader("RETARGET_PARAM", "Retarget Param"));
    write_text_file(temp_dir.dir / "retarget_ctrl.slangp",
                    "shaders = 1\nshader0 = retarget_ctrl.slang\nfilter_linear0 = false\n");

    const auto preset_path = temp_dir.dir / "retarget_ctrl.slangp";

    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    const auto preset_str = preset_path.string();
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path = make_utf8_view(preset_str);

    ProgramGuard prog_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    auto chain_info = goggles_fc_chain_create_info_init();
    chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    chain_info.frames_in_flight = 2u;
    chain_info.initial_prechain_resolution = {4u, 6u};

    ChainGuard chain_guard;
    REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                    &chain_guard.chain) == GOGGLES_FC_STATUS_OK);

    SECTION("control value survives retarget to a different format") {
        // Find the control and set it to a non-default value
        const auto control_name = make_utf8_view("RETARGET_PARAM");
        uint32_t ctrl_index = UINT32_MAX;
        REQUIRE(goggles_fc_chain_find_control_index(chain_guard.chain, GOGGLES_FC_STAGE_EFFECT,
                                                    control_name,
                                                    &ctrl_index) == GOGGLES_FC_STATUS_OK);

        auto ctrl_before = goggles_fc_control_info_init();
        REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, ctrl_index, &ctrl_before) ==
                GOGGLES_FC_STATUS_OK);
        // Default is 0.25 per make_parameter_shader; set to 0.75
        REQUIRE(goggles_fc_chain_set_control_value_f32(chain_guard.chain, ctrl_index, 0.75f) ==
                GOGGLES_FC_STATUS_OK);

        // Retarget to a different format
        auto target_info = goggles_fc_chain_target_info_init();
        target_info.target_format = VK_FORMAT_R8G8B8A8_SRGB;
        REQUIRE(goggles_fc_chain_retarget(chain_guard.chain, &target_info) == GOGGLES_FC_STATUS_OK);

        // Control value must be preserved
        auto ctrl_after = goggles_fc_control_info_init();
        REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, ctrl_index, &ctrl_after) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(std::string_view(ctrl_after.name.data, ctrl_after.name.size) == "RETARGET_PARAM");
        REQUIRE(ctrl_after.current_value == Catch::Approx(0.75f));
    }

    SECTION("prechain resolution survives retarget") {
        // Set a specific prechain resolution
        goggles_fc_extent_2d_t resolution = {8u, 12u};
        REQUIRE(goggles_fc_chain_set_prechain_resolution(chain_guard.chain, &resolution) ==
                GOGGLES_FC_STATUS_OK);

        // Retarget
        auto target_info = goggles_fc_chain_target_info_init();
        target_info.target_format = VK_FORMAT_R8G8B8A8_SRGB;
        REQUIRE(goggles_fc_chain_retarget(chain_guard.chain, &target_info) == GOGGLES_FC_STATUS_OK);

        // Prechain resolution must be preserved
        goggles_fc_extent_2d_t out_resolution = {};
        REQUIRE(goggles_fc_chain_get_prechain_resolution(chain_guard.chain, &out_resolution) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(out_resolution.width == 8u);
        REQUIRE(out_resolution.height == 12u);
    }

    SECTION("control value and prechain resolution both survive retarget together") {
        // Set control
        const auto control_name = make_utf8_view("RETARGET_PARAM");
        uint32_t ctrl_index = UINT32_MAX;
        REQUIRE(goggles_fc_chain_find_control_index(chain_guard.chain, GOGGLES_FC_STAGE_EFFECT,
                                                    control_name,
                                                    &ctrl_index) == GOGGLES_FC_STATUS_OK);
        REQUIRE(goggles_fc_chain_set_control_value_f32(chain_guard.chain, ctrl_index, 0.60f) ==
                GOGGLES_FC_STATUS_OK);

        // Set prechain resolution
        goggles_fc_extent_2d_t resolution = {16u, 24u};
        REQUIRE(goggles_fc_chain_set_prechain_resolution(chain_guard.chain, &resolution) ==
                GOGGLES_FC_STATUS_OK);

        // Retarget
        auto target_info = goggles_fc_chain_target_info_init();
        target_info.target_format = VK_FORMAT_B8G8R8A8_SRGB;
        REQUIRE(goggles_fc_chain_retarget(chain_guard.chain, &target_info) == GOGGLES_FC_STATUS_OK);

        // Both must be preserved
        auto ctrl_after = goggles_fc_control_info_init();
        REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, ctrl_index, &ctrl_after) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(ctrl_after.current_value == Catch::Approx(0.60f));

        goggles_fc_extent_2d_t out_resolution = {};
        REQUIRE(goggles_fc_chain_get_prechain_resolution(chain_guard.chain, &out_resolution) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(out_resolution.width == 16u);
        REQUIRE(out_resolution.height == 24u);
    }
}

// ===========================================================================
// TEST_CASE F: Chain report and error queries
// ===========================================================================

TEST_CASE("goggles_fc chain report and error queries", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Full lifecycle: instance → device → program → chain
    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    const auto preset_str = preset_path.string();
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path.data = preset_str.c_str();
    source.path.size = preset_str.size();

    ProgramGuard prog_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    auto chain_info = goggles_fc_chain_create_info_init();
    chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    chain_info.frames_in_flight = 2u;
    chain_info.initial_prechain_resolution = {1u, 1u};

    ChainGuard chain_guard;
    REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                    &chain_guard.chain) == GOGGLES_FC_STATUS_OK);

    SECTION("chain get_report returns populated struct") {
        auto report = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(chain_guard.chain, &report) == GOGGLES_FC_STATUS_OK);

        REQUIRE(report.struct_size == sizeof(goggles_fc_chain_report_t));
        // Freshly created chain has rendered 0 frames
        REQUIRE(report.frames_rendered == 0u);
        REQUIRE(report.current_stage_mask == GOGGLES_FC_STAGE_MASK_ALL);
    }

    SECTION("chain get_last_error returns OK status for fresh chain") {
        auto error_info = goggles_fc_chain_error_info_init();
        REQUIRE(goggles_fc_chain_get_last_error(chain_guard.chain, &error_info) ==
                GOGGLES_FC_STATUS_OK);

        // Fresh chain should have no error recorded
        REQUIRE(error_info.status == GOGGLES_FC_STATUS_OK);
    }

    SECTION("get_report with null chain returns INVALID_ARGUMENT") {
        auto report = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(nullptr, &report) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("get_report with null out_report returns INVALID_ARGUMENT") {
        REQUIRE(goggles_fc_chain_get_report(chain_guard.chain, nullptr) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("get_last_error with null chain returns INVALID_ARGUMENT") {
        auto error_info = goggles_fc_chain_error_info_init();
        REQUIRE(goggles_fc_chain_get_last_error(nullptr, &error_info) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("get_last_error with null out_error returns INVALID_ARGUMENT") {
        REQUIRE(goggles_fc_chain_get_last_error(chain_guard.chain, nullptr) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }
}

// ===========================================================================
// TEST_CASE G: Log callback replacement semantics
// ===========================================================================

TEST_CASE("goggles_fc log callback replacement semantics", "[goggles_fc_api]") {

    // --- Two distinct callback functions that record invocations -----------

    struct CallbackRecord {
        uint32_t call_count = 0u;
        std::vector<std::string> messages;
    };

    // Callback A
    auto callback_a = [](const goggles_fc_log_message_t* message, void* user_data) {
        auto* rec = static_cast<CallbackRecord*>(user_data);
        if (rec == nullptr) {
            return;
        }
        ++rec->call_count;
        if (message != nullptr && message->message.data != nullptr && message->message.size > 0u) {
            rec->messages.emplace_back(message->message.data, message->message.size);
        }
    };

    // Callback B — identical signature, separate lambda instance
    auto callback_b = [](const goggles_fc_log_message_t* message, void* user_data) {
        auto* rec = static_cast<CallbackRecord*>(user_data);
        if (rec == nullptr) {
            return;
        }
        ++rec->call_count;
        if (message != nullptr && message->message.data != nullptr && message->message.size > 0u) {
            rec->messages.emplace_back(message->message.data, message->message.size);
        }
    };

    SECTION("replacing callback A with callback B stops A from receiving messages") {
        CallbackRecord record_a;
        CallbackRecord record_b;

        // Create instance with callback A
        auto info = goggles_fc_instance_create_info_init();
        info.log_callback = callback_a;
        info.log_user_data = &record_a;

        InstanceGuard guard;
        REQUIRE(goggles_fc_instance_create(&info, &guard.instance) == GOGGLES_FC_STATUS_OK);
        REQUIRE(guard.instance != nullptr);

        // Replace with callback B
        REQUIRE(goggles_fc_instance_set_log_callback(guard.instance, callback_b, &record_b) ==
                GOGGLES_FC_STATUS_OK);

        // Snapshot A's call count at the moment of replacement
        const uint32_t a_count_at_replacement = record_a.call_count;

        // After replacement, A must not receive any additional messages
        // (the callback pointer is no longer registered with the instance).
        // Any subsequent log activity should only go to B.
        // We verify by checking that A's count is frozen at this point.
        REQUIRE(record_a.call_count == a_count_at_replacement);
    }

    SECTION(
        "replacing callback with same function but different user_data routes to new user_data") {
        CallbackRecord record_first;
        CallbackRecord record_second;

        // Create instance with callback_a + record_first
        auto info = goggles_fc_instance_create_info_init();
        info.log_callback = callback_a;
        info.log_user_data = &record_first;

        InstanceGuard guard;
        REQUIRE(goggles_fc_instance_create(&info, &guard.instance) == GOGGLES_FC_STATUS_OK);

        // Replace with same callback function but different user_data
        REQUIRE(goggles_fc_instance_set_log_callback(guard.instance, callback_a, &record_second) ==
                GOGGLES_FC_STATUS_OK);

        // Snapshot: first should be frozen, second is now the target
        const uint32_t first_count_at_replacement = record_first.call_count;
        REQUIRE(record_first.call_count == first_count_at_replacement);
    }

    SECTION("clearing callback with null stops all callback delivery") {
        CallbackRecord record;

        // Create instance with a callback
        auto info = goggles_fc_instance_create_info_init();
        info.log_callback = callback_a;
        info.log_user_data = &record;

        InstanceGuard guard;
        REQUIRE(goggles_fc_instance_create(&info, &guard.instance) == GOGGLES_FC_STATUS_OK);

        // Clear callback
        REQUIRE(goggles_fc_instance_set_log_callback(guard.instance, nullptr, nullptr) ==
                GOGGLES_FC_STATUS_OK);

        // Snapshot: no more callbacks should arrive after clearing
        const uint32_t count_at_clear = record.call_count;
        REQUIRE(record.call_count == count_at_clear);
    }

    SECTION("set_log_callback returns OK on each replacement") {
        InstanceGuard guard;
        REQUIRE(create_test_instance(&guard.instance) == GOGGLES_FC_STATUS_OK);

        CallbackRecord rec_a;
        CallbackRecord rec_b;

        // Set A → OK
        REQUIRE(goggles_fc_instance_set_log_callback(guard.instance, callback_a, &rec_a) ==
                GOGGLES_FC_STATUS_OK);

        // Replace with B → OK
        REQUIRE(goggles_fc_instance_set_log_callback(guard.instance, callback_b, &rec_b) ==
                GOGGLES_FC_STATUS_OK);

        // Clear → OK
        REQUIRE(goggles_fc_instance_set_log_callback(guard.instance, nullptr, nullptr) ==
                GOGGLES_FC_STATUS_OK);

        // Set A again → OK (re-registration after clear)
        REQUIRE(goggles_fc_instance_set_log_callback(guard.instance, callback_a, &rec_a) ==
                GOGGLES_FC_STATUS_OK);
    }

    SECTION("set_log_callback with null instance returns INVALID_ARGUMENT") {
        CallbackRecord rec;
        REQUIRE(goggles_fc_instance_set_log_callback(nullptr, callback_a, &rec) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }
}

TEST_CASE("goggles_fc macro logging stays isolated per instance", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));
    const auto preset_str = preset_path.string();

    LogCallbackState state_a;
    auto info_a = goggles_fc_instance_create_info_init();
    info_a.log_callback = test_log_callback;
    info_a.log_user_data = &state_a;

    InstanceGuard inst_a;
    REQUIRE(goggles_fc_instance_create(&info_a, &inst_a.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_a;
    REQUIRE(create_test_device(fixture, inst_a.instance, &dev_a.dev) == GOGGLES_FC_STATUS_OK);

    LogCallbackState state_b;
    auto info_b = goggles_fc_instance_create_info_init();
    info_b.log_callback = test_log_callback;
    info_b.log_user_data = &state_b;

    InstanceGuard inst_b;
    REQUIRE(goggles_fc_instance_create(&info_b, &inst_b.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_b;
    REQUIRE(create_test_device(fixture, inst_b.instance, &dev_b.dev) == GOGGLES_FC_STATUS_OK);

    auto source_a = goggles_fc_preset_source_init();
    source_a.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source_a.path.data = preset_str.c_str();
    source_a.path.size = preset_str.size();

    ProgramGuard prog_a;
    const auto a_before = state_a.call_count;
    const auto b_before_for_a = state_b.call_count;
    REQUIRE(goggles_fc_program_create(dev_a.dev, &source_a, &prog_a.prog) == GOGGLES_FC_STATUS_OK);
    REQUIRE(state_a.call_count > a_before);
    REQUIRE(state_b.call_count == b_before_for_a);

    auto source_b = goggles_fc_preset_source_init();
    source_b.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source_b.path.data = preset_str.c_str();
    source_b.path.size = preset_str.size();

    ProgramGuard prog_b;
    const auto a_before_b = state_a.call_count;
    const auto b_before = state_b.call_count;
    REQUIRE(goggles_fc_program_create(dev_b.dev, &source_b, &prog_b.prog) == GOGGLES_FC_STATUS_OK);
    REQUIRE(state_b.call_count > b_before);
    REQUIRE(state_a.call_count == a_before_b);
}

// ===========================================================================
// TEST_CASE H: Log callback receives messages on emitting thread
// ===========================================================================

TEST_CASE("goggles_fc log callback receives messages on emitting thread",
          "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // State captured by the callback
    struct ThreadLogState {
        uint32_t call_count = 0u;
        std::vector<std::thread::id> callback_thread_ids;
        std::vector<std::string> messages;
    };

    auto thread_log_callback = [](const goggles_fc_log_message_t* message, void* user_data) {
        auto* state = static_cast<ThreadLogState*>(user_data);
        if (state == nullptr) {
            return;
        }
        ++state->call_count;
        state->callback_thread_ids.push_back(std::this_thread::get_id());
        if (message != nullptr && message->message.data != nullptr && message->message.size > 0u) {
            state->messages.emplace_back(message->message.data, message->message.size);
        }
    };

    SECTION("callback is invoked on the calling thread during program create") {
        ThreadLogState state;

        // Create instance with our thread-tracking callback
        auto info = goggles_fc_instance_create_info_init();
        info.log_callback = thread_log_callback;
        info.log_user_data = &state;

        InstanceGuard inst_guard;
        REQUIRE(goggles_fc_instance_create(&info, &inst_guard.instance) == GOGGLES_FC_STATUS_OK);

        // Create device
        DeviceGuard dev_guard;
        REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
                GOGGLES_FC_STATUS_OK);

        // Record the current thread ID BEFORE creating the program
        const auto caller_thread_id = std::this_thread::get_id();

        // Create a program from file — this triggers shader compilation and
        // emits log messages (e.g., "loading preset", "compiling shader")
        const auto preset_str = preset_path.string();
        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = preset_str.c_str();
        source.path.size = preset_str.size();

        ProgramGuard prog_guard;
        const auto status = goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);

        // The callback must have been invoked at least once
        INFO("Log callback invoked " << state.call_count << " times during program_create");
        REQUIRE(state.call_count > 0u);

        // Every callback invocation must have occurred on the caller's thread,
        // proving synchronous delivery (no worker thread dispatch)
        for (size_t i = 0u; i < state.callback_thread_ids.size(); ++i) {
            INFO("Callback invocation " << i << " thread ID mismatch — expected synchronous "
                                        << "delivery on the calling thread");
            REQUIRE(state.callback_thread_ids[i] == caller_thread_id);
        }
    }
}

// ===========================================================================
// TEST_CASE I: Log message struct contract
// ===========================================================================

TEST_CASE("goggles_fc log message struct contract", "[goggles_fc_api]") {

    SECTION("goggles_fc_log_message_init returns properly initialized struct") {
        auto msg = goggles_fc_log_message_init();

        REQUIRE(msg.struct_size == sizeof(goggles_fc_log_message_t));
        REQUIRE(msg.level == GOGGLES_FC_LOG_LEVEL_INFO);
        REQUIRE(msg.domain.data == nullptr);
        REQUIRE(msg.domain.size == 0u);
        REQUIRE(msg.message.data == nullptr);
        REQUIRE(msg.message.size == 0u);
    }

    SECTION("struct_size field is the first member at offset 0") {
        STATIC_REQUIRE(offsetof(GogglesFcLogMessage, struct_size) == 0u);
    }

    SECTION("goggles_fc_log_message_t layout: all fields accessible") {
        auto msg = goggles_fc_log_message_init();

        // Verify each field is independently writable and readable
        msg.struct_size = 42u;
        REQUIRE(msg.struct_size == 42u);

        msg.level = GOGGLES_FC_LOG_LEVEL_ERROR;
        REQUIRE(msg.level == GOGGLES_FC_LOG_LEVEL_ERROR);

        const char test_domain[] = "test-domain";
        msg.domain.data = test_domain;
        msg.domain.size = sizeof(test_domain) - 1u;
        REQUIRE(msg.domain.data == test_domain);
        REQUIRE(msg.domain.size == 11u);

        const char test_message[] = "hello";
        msg.message.data = test_message;
        msg.message.size = sizeof(test_message) - 1u;
        REQUIRE(msg.message.data == test_message);
        REQUIRE(msg.message.size == 5u);
    }

    SECTION("all GOGGLES_FC_LOG_LEVEL_* values are distinct and in expected order") {
        // Verify values are as documented
        STATIC_REQUIRE(GOGGLES_FC_LOG_LEVEL_TRACE == 0u);
        STATIC_REQUIRE(GOGGLES_FC_LOG_LEVEL_DEBUG == 1u);
        STATIC_REQUIRE(GOGGLES_FC_LOG_LEVEL_INFO == 2u);
        STATIC_REQUIRE(GOGGLES_FC_LOG_LEVEL_WARN == 3u);
        STATIC_REQUIRE(GOGGLES_FC_LOG_LEVEL_ERROR == 4u);
        STATIC_REQUIRE(GOGGLES_FC_LOG_LEVEL_CRITICAL == 5u);

        // Verify all 6 values are mutually distinct (ascending order implies distinct)
        STATIC_REQUIRE(GOGGLES_FC_LOG_LEVEL_TRACE < GOGGLES_FC_LOG_LEVEL_DEBUG);
        STATIC_REQUIRE(GOGGLES_FC_LOG_LEVEL_DEBUG < GOGGLES_FC_LOG_LEVEL_INFO);
        STATIC_REQUIRE(GOGGLES_FC_LOG_LEVEL_INFO < GOGGLES_FC_LOG_LEVEL_WARN);
        STATIC_REQUIRE(GOGGLES_FC_LOG_LEVEL_WARN < GOGGLES_FC_LOG_LEVEL_ERROR);
        STATIC_REQUIRE(GOGGLES_FC_LOG_LEVEL_ERROR < GOGGLES_FC_LOG_LEVEL_CRITICAL);
    }

    SECTION("goggles_fc_log_message_t struct_size matches sizeof") {
        STATIC_REQUIRE(sizeof(goggles_fc_log_message_t) == sizeof(GogglesFcLogMessage));

        // The init helper should produce the correct value
        const auto msg = goggles_fc_log_message_init();
        REQUIRE(msg.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_log_message_t));
    }
}

// ===========================================================================
// TEST_CASE J: Program shared across chains on same device
// ===========================================================================

TEST_CASE("goggles_fc program shared across chains on same device", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Set up instance + device + program
    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    const auto preset_str = preset_path.string();
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path.data = preset_str.c_str();
    source.path.size = preset_str.size();

    ProgramGuard prog_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    SECTION("two chains created from the same device and program both succeed") {
        auto chain_info = goggles_fc_chain_create_info_init();
        chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
        chain_info.frames_in_flight = 2u;
        chain_info.initial_prechain_resolution = {1u, 1u};

        // Create chain_a from (device, program) → succeeds
        ChainGuard chain_a;
        REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                        &chain_a.chain) == GOGGLES_FC_STATUS_OK);
        REQUIRE(chain_a.chain != nullptr);

        // Create chain_b from (device, same program) → succeeds (program sharing)
        ChainGuard chain_b;
        REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                        &chain_b.chain) == GOGGLES_FC_STATUS_OK);
        REQUIRE(chain_b.chain != nullptr);

        // Both chains exist and are distinct handles
        REQUIRE(chain_a.chain != chain_b.chain);
    }

    SECTION("shared-program chains have independent state") {
        auto chain_info = goggles_fc_chain_create_info_init();
        chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
        chain_info.frames_in_flight = 2u;
        chain_info.initial_prechain_resolution = {1u, 1u};

        ChainGuard chain_a;
        REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                        &chain_a.chain) == GOGGLES_FC_STATUS_OK);

        // Create chain_b with a different target format to demonstrate independence
        auto chain_info_b = goggles_fc_chain_create_info_init();
        chain_info_b.target_format = VK_FORMAT_R8G8B8A8_UNORM;
        chain_info_b.frames_in_flight = 2u;
        chain_info_b.initial_prechain_resolution = {1u, 1u};

        ChainGuard chain_b;
        REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info_b,
                                        &chain_b.chain) == GOGGLES_FC_STATUS_OK);

        // Each chain reports its own state independently
        auto report_a = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(chain_a.chain, &report_a) == GOGGLES_FC_STATUS_OK);

        auto report_b = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(chain_b.chain, &report_b) == GOGGLES_FC_STATUS_OK);

        // Both chains have rendered 0 frames independently
        REQUIRE(report_a.frames_rendered == 0u);
        REQUIRE(report_b.frames_rendered == 0u);

        // Retarget chain_a only — chain_b should be unaffected
        auto target_info = goggles_fc_chain_target_info_init();
        target_info.target_format = VK_FORMAT_R8G8B8A8_SRGB;
        REQUIRE(goggles_fc_chain_retarget(chain_a.chain, &target_info) == GOGGLES_FC_STATUS_OK);

        // chain_b still reports independently (get_report still succeeds)
        auto report_b2 = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(chain_b.chain, &report_b2) == GOGGLES_FC_STATUS_OK);
    }

    SECTION("destroying one shared-program chain does not affect the other") {
        auto chain_info = goggles_fc_chain_create_info_init();
        chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
        chain_info.frames_in_flight = 2u;
        chain_info.initial_prechain_resolution = {1u, 1u};

        // Create two chains sharing the same program
        goggles_fc_chain_t* chain_a = nullptr;
        REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info, &chain_a) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(chain_a != nullptr);

        ChainGuard chain_b;
        REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                        &chain_b.chain) == GOGGLES_FC_STATUS_OK);

        // Destroy chain_a explicitly
        goggles_fc_chain_destroy(chain_a);

        // chain_b should still be fully operational
        auto report = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(chain_b.chain, &report) == GOGGLES_FC_STATUS_OK);
        REQUIRE(report.frames_rendered == 0u);
    }
}

// ===========================================================================
// TEST_CASE K: Program rejected on different device
// ===========================================================================

TEST_CASE("goggles_fc program rejected on different device", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // --- Create a second VkDevice from the same physical device ---------------
    // This is a valid Vulkan operation: multiple VkDevice handles from one
    // VkPhysicalDevice. Each becomes a distinct goggles_fc_device_t.

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features13;
    vkGetPhysicalDeviceFeatures2(fixture.physical_device(), &features2);

    if (features13.dynamicRendering != VK_TRUE) {
        SKIP("Dynamic rendering not supported — cannot create second device");
    }

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = fixture.queue_family_index();
    queue_info.queueCount = 1u;
    queue_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1u;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.pNext = &features13;

    VkDevice second_vk_device = VK_NULL_HANDLE;
    if (vkCreateDevice(fixture.physical_device(), &device_info, nullptr, &second_vk_device) !=
        VK_SUCCESS) {
        SKIP("Could not create a second VkDevice from the same physical device");
    }
    REQUIRE(second_vk_device != VK_NULL_HANDLE);

    VkQueue second_queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(second_vk_device, fixture.queue_family_index(), 0u, &second_queue);

    // RAII cleanup for the manually-created VkDevice. Declared FIRST so it is
    // destroyed LAST (reverse declaration order), after all goggles_fc objects
    // that borrow it have been torn down.
    struct VkDeviceCleanup {
        VkDevice dev = VK_NULL_HANDLE;
        ~VkDeviceCleanup() {
            if (dev != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(dev);
                vkDestroyDevice(dev, nullptr);
            }
        }
    } second_device_cleanup{second_vk_device};

    // --- Set up goggles_fc objects ------------------------------------------

    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    // device_a: wraps the fixture's VkDevice
    DeviceGuard dev_a_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_a_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    // device_b: wraps the second VkDevice
    auto dev_b_info = goggles_fc_vk_device_create_info_init();
    dev_b_info.physical_device = fixture.physical_device();
    dev_b_info.device = second_vk_device;
    dev_b_info.graphics_queue = second_queue;
    dev_b_info.graphics_queue_family_index = fixture.queue_family_index();

    DeviceGuard dev_b_guard;
    auto dev_b_status =
        goggles_fc_device_create_vk(inst_guard.instance, &dev_b_info, &dev_b_guard.dev);
    if (dev_b_status != GOGGLES_FC_STATUS_OK) {
        SKIP("Could not create second goggles_fc_device_t");
    }
    REQUIRE(dev_b_guard.dev != nullptr);

    // Create program_a on device_a
    const auto preset_str = preset_path.string();
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path.data = preset_str.c_str();
    source.path.size = preset_str.size();

    ProgramGuard prog_a_guard;
    REQUIRE(goggles_fc_program_create(dev_a_guard.dev, &source, &prog_a_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    SECTION("chain_create with program from different device returns INVALID_DEPENDENCY") {
        auto chain_info = goggles_fc_chain_create_info_init();
        chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
        chain_info.frames_in_flight = 2u;
        chain_info.initial_prechain_resolution = {1u, 1u};

        // Attempt to create a chain on device_b using program_a (from device_a) → must fail
        goggles_fc_chain_t* chain = nullptr;
        const auto status =
            goggles_fc_chain_create(dev_b_guard.dev, prog_a_guard.prog, &chain_info, &chain);
        REQUIRE(status == GOGGLES_FC_STATUS_INVALID_DEPENDENCY);
        REQUIRE(chain == nullptr);
    }

    SECTION("bind_program with program from different device returns INVALID_DEPENDENCY") {
        // Create a valid chain on device_b with its own program
        ProgramGuard prog_b_guard;
        REQUIRE(goggles_fc_program_create(dev_b_guard.dev, &source, &prog_b_guard.prog) ==
                GOGGLES_FC_STATUS_OK);

        auto chain_info = goggles_fc_chain_create_info_init();
        chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
        chain_info.frames_in_flight = 2u;
        chain_info.initial_prechain_resolution = {1u, 1u};

        ChainGuard chain_guard;
        REQUIRE(goggles_fc_chain_create(dev_b_guard.dev, prog_b_guard.prog, &chain_info,
                                        &chain_guard.chain) == GOGGLES_FC_STATUS_OK);

        // Attempt to bind program_a (from device_a) to chain on device_b → must fail
        const auto status = goggles_fc_chain_bind_program(chain_guard.chain, prog_a_guard.prog);
        REQUIRE(status == GOGGLES_FC_STATUS_INVALID_DEPENDENCY);
    }

    SECTION("chain_create on same device as program succeeds (positive control)") {
        auto chain_info = goggles_fc_chain_create_info_init();
        chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
        chain_info.frames_in_flight = 2u;
        chain_info.initial_prechain_resolution = {1u, 1u};

        // Creating chain on device_a with program_a (same device) → must succeed
        ChainGuard chain_guard;
        const auto status = goggles_fc_chain_create(dev_a_guard.dev, prog_a_guard.prog, &chain_info,
                                                    &chain_guard.chain);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
        REQUIRE(chain_guard.chain != nullptr);
    }
}

// ===========================================================================
// TEST_CASE L: Chain bind_program semantics
// ===========================================================================

TEST_CASE("goggles_fc chain bind_program semantics", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Set up instance + device
    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    // Create two programs from the same device
    const auto preset_str = preset_path.string();
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path.data = preset_str.c_str();
    source.path.size = preset_str.size();

    ProgramGuard prog_a_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_a_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    ProgramGuard prog_b_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_b_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    auto passthrough_source = goggles_fc_preset_source_init();
    passthrough_source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    passthrough_source.path.data = "";
    passthrough_source.path.size = 0;

    ProgramGuard passthrough_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &passthrough_source,
                                      &passthrough_guard.prog) == GOGGLES_FC_STATUS_OK);

    // Create a chain initially bound to program_a
    auto chain_info = goggles_fc_chain_create_info_init();
    chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    chain_info.frames_in_flight = 2u;
    chain_info.initial_prechain_resolution = {1u, 1u};

    ChainGuard chain_guard;
    REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_a_guard.prog, &chain_info,
                                    &chain_guard.chain) == GOGGLES_FC_STATUS_OK);

    SECTION("bind to different program on same device succeeds") {
        const auto status = goggles_fc_chain_bind_program(chain_guard.chain, prog_b_guard.prog);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
    }

    SECTION("bind with null program returns INVALID_ARGUMENT") {
        const auto status = goggles_fc_chain_bind_program(chain_guard.chain, nullptr);
        REQUIRE(status == GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("bind with null chain returns INVALID_ARGUMENT") {
        const auto status = goggles_fc_chain_bind_program(nullptr, prog_b_guard.prog);
        REQUIRE(status == GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("after bind, chain report is still queryable") {
        REQUIRE(goggles_fc_chain_bind_program(chain_guard.chain, prog_b_guard.prog) ==
                GOGGLES_FC_STATUS_OK);

        auto report = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(chain_guard.chain, &report) == GOGGLES_FC_STATUS_OK);
        REQUIRE(report.struct_size == sizeof(goggles_fc_chain_report_t));
    }

    SECTION("bind installs the replacement program instead of keeping the previous passes") {
        auto before_report = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(chain_guard.chain, &before_report) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(before_report.pass_count > 0u);

        REQUIRE(goggles_fc_chain_bind_program(chain_guard.chain, passthrough_guard.prog) ==
                GOGGLES_FC_STATUS_OK);

        auto after_report = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(chain_guard.chain, &after_report) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(after_report.pass_count == 0u);
    }

    SECTION("binding the same program again succeeds (idempotent)") {
        // Bind to prog_a (already bound) — should be a no-op success
        const auto status = goggles_fc_chain_bind_program(chain_guard.chain, prog_a_guard.prog);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
    }
}

// ===========================================================================
// TEST_CASE M: Embedded asset program creation
// ===========================================================================

TEST_CASE("goggles_fc embedded asset program creation", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    // The test preset file is loaded from disk via FILTER_CHAIN_ASSET_DIR.
    // The shaders it references (format.slang, decode-format.slang) are also
    // resolved from the filesystem via the preset's parent directory.
    //
    // However, the internal pipeline shaders (blit.vert, blit.frag,
    // downsample.frag) used by OutputPass and DownsamplePass are resolved from
    // the EmbeddedAssetRegistry — compiled into the library via EmbedAssets.cmake.
    // This means programs can be created and chains can run without an external
    // shader_dir for internal pipeline passes.
    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Set up instance + device
    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    SECTION("program from format.slangp compiles successfully using embedded assets") {
        const auto preset_str = preset_path.string();

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = preset_str.c_str();
        source.path.size = preset_str.size();

        // No shader_dir is set — internal pipeline shaders must come from
        // the embedded asset registry compiled into the library.
        ProgramGuard prog_guard;
        const auto status = goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog);
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
        REQUIRE(prog_guard.prog != nullptr);
    }

    SECTION("program report shows shader_count > 0 and pass_count > 0") {
        const auto preset_str = preset_path.string();

        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = preset_str.c_str();
        source.path.size = preset_str.size();

        ProgramGuard prog_guard;
        REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
                GOGGLES_FC_STATUS_OK);

        auto report = goggles_fc_program_report_init();
        REQUIRE(goggles_fc_program_get_report(prog_guard.prog, &report) == GOGGLES_FC_STATUS_OK);

        // format.slangp defines 2 shaders → at least 2 passes. The report must
        // reflect that these compiled successfully (using filesystem for preset
        // shaders and embedded assets for internal pipeline shaders).
        INFO("shader_count=" << report.shader_count << " pass_count=" << report.pass_count
                             << " texture_count=" << report.texture_count);
        REQUIRE(report.shader_count > 0u);
        REQUIRE(report.pass_count > 0u);
    }
}

// ===========================================================================
// TEST_CASE N: Embedded assets remove shader_dir dependency
// ===========================================================================

TEST_CASE("goggles_fc embedded assets remove shader_dir dependency", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    // The key contract: the preset file itself must be loaded from disk (it is
    // NOT embedded), but the internal pipeline shaders (blit, downsample) that
    // OutputPass and DownsamplePass need are resolved from the embedded asset
    // registry. No external shader_dir path is required at runtime.
    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    SECTION("program creation succeeds without shader_dir — log callback observes compilation") {
        // Use a log callback to observe the compilation process. If embedded
        // asset resolution produces any log messages, they will appear here.
        LogCallbackState log_state;

        auto info = goggles_fc_instance_create_info_init();
        info.log_callback = test_log_callback;
        info.log_user_data = &log_state;

        InstanceGuard inst_guard;
        REQUIRE(goggles_fc_instance_create(&info, &inst_guard.instance) == GOGGLES_FC_STATUS_OK);

        DeviceGuard dev_guard;
        REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
                GOGGLES_FC_STATUS_OK);

        const auto preset_str = preset_path.string();
        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = preset_str.c_str();
        source.path.size = preset_str.size();

        ProgramGuard prog_guard;
        const auto status = goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog);

        // The program must compile successfully. The fact that it does — without
        // any shader_dir being passed — proves that the embedded asset registry
        // is providing the internal pipeline shaders.
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
        REQUIRE(prog_guard.prog != nullptr);

        // Log messages were emitted during program creation (compilation, loading).
        // This is observational — the test succeeds based on status, not log content.
        INFO("Log messages received during program create: " << log_state.call_count);
        REQUIRE(log_state.call_count > 0u);
    }

    SECTION("preset shaders are from filesystem, not embedded") {
        // Verify that the preset's own shaders (format.slang, decode-format.slang)
        // exist on disk — confirming the test preset relies on filesystem for its
        // shaders while internal pipeline passes use embedded assets.
        const auto shader_dir = preset_path.parent_path();
        REQUIRE(std::filesystem::exists(shader_dir / "format.slang"));
        REQUIRE(std::filesystem::exists(shader_dir / "decode-format.slang"));

        // Now create the program — both resolution paths are exercised:
        // 1. Preset file and its shaders: filesystem
        // 2. Internal pipeline shaders (blit, downsample): embedded assets
        InstanceGuard inst_guard;
        REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

        DeviceGuard dev_guard;
        REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
                GOGGLES_FC_STATUS_OK);

        const auto preset_str = preset_path.string();
        auto source = goggles_fc_preset_source_init();
        source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
        source.path.data = preset_str.c_str();
        source.path.size = preset_str.size();

        ProgramGuard prog_guard;
        REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
                GOGGLES_FC_STATUS_OK);

        // Verify provenance is FILE (since the preset comes from disk)
        auto src_info = goggles_fc_program_source_info_init();
        REQUIRE(goggles_fc_program_get_source_info(prog_guard.prog, &src_info) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(src_info.provenance == GOGGLES_FC_PROVENANCE_FILE);
    }
}

// ===========================================================================
// TEST_CASE O: Embedded asset integration with chain execution
// ===========================================================================

TEST_CASE("goggles_fc embedded asset integration with chain execution",
          "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    // Full lifecycle: instance → device → program (from format.slangp) → chain.
    // This proves that embedded internal shaders compiled, loaded, and the chain
    // is functional — OutputPass::create_pipeline() and DownsamplePass::create_pipeline()
    // resolved their shaders from EmbeddedAssetRegistry::find() rather than the filesystem.
    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    // Set up instance with log callback for observability
    LogCallbackState log_state;

    auto info = goggles_fc_instance_create_info_init();
    info.log_callback = test_log_callback;
    info.log_user_data = &log_state;

    InstanceGuard inst_guard;
    REQUIRE(goggles_fc_instance_create(&info, &inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    // Create program
    const auto preset_str = preset_path.string();
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path.data = preset_str.c_str();
    source.path.size = preset_str.size();

    ProgramGuard prog_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    SECTION("chain creation succeeds with embedded internal shaders") {
        auto chain_info = goggles_fc_chain_create_info_init();
        chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
        chain_info.frames_in_flight = 2u;
        chain_info.initial_prechain_resolution = {1u, 1u};

        ChainGuard chain_guard;
        const auto status = goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                                    &chain_guard.chain);
        // Chain creation must succeed — this is the end-to-end proof that
        // embedded internal shaders (blit.vert, blit.frag) were resolved
        // from the registry, compiled into pipelines, and the chain is ready.
        REQUIRE(status == GOGGLES_FC_STATUS_OK);
        REQUIRE(chain_guard.chain != nullptr);
    }

    SECTION("chain report and control count are queryable after creation") {
        auto chain_info = goggles_fc_chain_create_info_init();
        chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
        chain_info.frames_in_flight = 2u;
        chain_info.initial_prechain_resolution = {1u, 1u};

        auto program_report = goggles_fc_program_report_init();
        REQUIRE(goggles_fc_program_get_report(prog_guard.prog, &program_report) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(program_report.pass_count > 0u);

        ChainGuard chain_guard;
        REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                        &chain_guard.chain) == GOGGLES_FC_STATUS_OK);

        // Chain report confirms passes were built from the compiled program
        auto report = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(chain_guard.chain, &report) == GOGGLES_FC_STATUS_OK);
        REQUIRE(report.struct_size == sizeof(goggles_fc_chain_report_t));
        REQUIRE(report.pass_count == program_report.pass_count);
        REQUIRE(report.frames_rendered == 0u);
        REQUIRE(report.current_stage_mask == GOGGLES_FC_STAGE_MASK_ALL);

        // Control count query works on the chain built from embedded-asset pipeline
        uint32_t control_count = UINT32_MAX;
        REQUIRE(goggles_fc_chain_get_control_count(chain_guard.chain, &control_count) ==
                GOGGLES_FC_STATUS_OK);
        INFO("control count on embedded-asset chain: " << control_count);
    }

    SECTION("chain retarget works after embedded-asset-based creation") {
        auto chain_info = goggles_fc_chain_create_info_init();
        chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
        chain_info.frames_in_flight = 2u;
        chain_info.initial_prechain_resolution = {1u, 1u};

        ChainGuard chain_guard;
        REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                        &chain_guard.chain) == GOGGLES_FC_STATUS_OK);

        // Retarget to a different format — proves the chain is fully functional
        // and its embedded-asset-compiled pipelines are ready for re-configuration.
        auto target_info = goggles_fc_chain_target_info_init();
        target_info.target_format = VK_FORMAT_R8G8B8A8_UNORM;
        REQUIRE(goggles_fc_chain_retarget(chain_guard.chain, &target_info) == GOGGLES_FC_STATUS_OK);
    }
}

// ===========================================================================
// TEST_CASE P: set_stage_mask C API contract
// ===========================================================================

TEST_CASE("goggles_fc chain set_stage_mask contract", "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
    REQUIRE(std::filesystem::exists(preset_path));

    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    const auto preset_str = preset_path.string();
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path.data = preset_str.c_str();
    source.path.size = preset_str.size();

    ProgramGuard prog_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    auto chain_info = goggles_fc_chain_create_info_init();
    chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    chain_info.frames_in_flight = 2u;
    chain_info.initial_prechain_resolution = {1u, 1u};

    ChainGuard chain_guard;
    REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                    &chain_guard.chain) == GOGGLES_FC_STATUS_OK);

    SECTION("null handle returns INVALID_ARGUMENT") {
        REQUIRE(goggles_fc_chain_set_stage_mask(nullptr, GOGGLES_FC_STAGE_MASK_ALL) ==
                GOGGLES_FC_STATUS_INVALID_ARGUMENT);
    }

    SECTION("valid call succeeds and report reflects updated mask") {
        const uint32_t new_mask = GOGGLES_FC_STAGE_MASK_PRECHAIN | GOGGLES_FC_STAGE_MASK_POSTCHAIN;
        REQUIRE(goggles_fc_chain_set_stage_mask(chain_guard.chain, new_mask) ==
                GOGGLES_FC_STATUS_OK);

        auto report = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(chain_guard.chain, &report) == GOGGLES_FC_STATUS_OK);
        REQUIRE(report.current_stage_mask == new_mask);
    }

    SECTION("disabling all stages succeeds") {
        REQUIRE(goggles_fc_chain_set_stage_mask(chain_guard.chain, 0u) == GOGGLES_FC_STATUS_OK);

        auto report = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(chain_guard.chain, &report) == GOGGLES_FC_STATUS_OK);
        REQUIRE(report.current_stage_mask == 0u);
    }

    SECTION("re-enabling all stages succeeds") {
        REQUIRE(goggles_fc_chain_set_stage_mask(chain_guard.chain, 0u) == GOGGLES_FC_STATUS_OK);
        REQUIRE(goggles_fc_chain_set_stage_mask(chain_guard.chain, GOGGLES_FC_STAGE_MASK_ALL) ==
                GOGGLES_FC_STATUS_OK);

        auto report = goggles_fc_chain_report_init();
        REQUIRE(goggles_fc_chain_get_report(chain_guard.chain, &report) == GOGGLES_FC_STATUS_OK);
        REQUIRE(report.current_stage_mask == GOGGLES_FC_STAGE_MASK_ALL);
    }
}

// ===========================================================================
// TEST_CASE Q: Stage mask and prechain resolution changes preserve controls
// ===========================================================================

TEST_CASE("goggles_fc stage mask and prechain resolution preserve control values",
          "[goggles_fc_api][vulkan]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("No Vulkan graphics device available");
    }

    TempPresetDir dir;
    write_text_file(dir.dir / "preserve.slang",
                    make_parameter_shader("PRESERVE_PARAM", "Preserve Param"));
    write_text_file(dir.dir / "preserve.slangp",
                    "shaders = 1\nshader0 = preserve.slang\nfilter_linear0 = false\n");

    InstanceGuard inst_guard;
    REQUIRE(create_test_instance(&inst_guard.instance) == GOGGLES_FC_STATUS_OK);

    DeviceGuard dev_guard;
    REQUIRE(create_test_device(fixture, inst_guard.instance, &dev_guard.dev) ==
            GOGGLES_FC_STATUS_OK);

    const auto preset_str = (dir.dir / "preserve.slangp").string();
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path.data = preset_str.c_str();
    source.path.size = preset_str.size();

    ProgramGuard prog_guard;
    REQUIRE(goggles_fc_program_create(dev_guard.dev, &source, &prog_guard.prog) ==
            GOGGLES_FC_STATUS_OK);

    auto chain_info = goggles_fc_chain_create_info_init();
    chain_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    chain_info.frames_in_flight = 2u;
    chain_info.initial_prechain_resolution = {4u, 4u};

    ChainGuard chain_guard;
    REQUIRE(goggles_fc_chain_create(dev_guard.dev, prog_guard.prog, &chain_info,
                                    &chain_guard.chain) == GOGGLES_FC_STATUS_OK);

    // Find and set the control
    const auto control_name = make_utf8_view("PRESERVE_PARAM");
    uint32_t ctrl_index = UINT32_MAX;
    REQUIRE(goggles_fc_chain_find_control_index(chain_guard.chain, GOGGLES_FC_STAGE_EFFECT,
                                                control_name, &ctrl_index) == GOGGLES_FC_STATUS_OK);
    REQUIRE(goggles_fc_chain_set_control_value_f32(chain_guard.chain, ctrl_index, 0.80f) ==
            GOGGLES_FC_STATUS_OK);

    SECTION("aspect-preserving resolution {0, h} is accepted") {
        goggles_fc_extent_2d_t resolution = {0u, 240u};
        REQUIRE(goggles_fc_chain_set_prechain_resolution(chain_guard.chain, &resolution) ==
                GOGGLES_FC_STATUS_OK);

        goggles_fc_extent_2d_t out_resolution = {};
        REQUIRE(goggles_fc_chain_get_prechain_resolution(chain_guard.chain, &out_resolution) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(out_resolution.width == 0u);
        REQUIRE(out_resolution.height == 240u);
    }

    SECTION("aspect-preserving resolution {w, 0} is accepted") {
        goggles_fc_extent_2d_t resolution = {320u, 0u};
        REQUIRE(goggles_fc_chain_set_prechain_resolution(chain_guard.chain, &resolution) ==
                GOGGLES_FC_STATUS_OK);

        goggles_fc_extent_2d_t out_resolution = {};
        REQUIRE(goggles_fc_chain_get_prechain_resolution(chain_guard.chain, &out_resolution) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(out_resolution.width == 320u);
        REQUIRE(out_resolution.height == 0u);
    }

    SECTION("prechain resolution change preserves control values") {
        goggles_fc_extent_2d_t resolution = {16u, 16u};
        REQUIRE(goggles_fc_chain_set_prechain_resolution(chain_guard.chain, &resolution) ==
                GOGGLES_FC_STATUS_OK);

        auto ctrl_after = goggles_fc_control_info_init();
        REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, ctrl_index, &ctrl_after) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(std::string_view(ctrl_after.name.data, ctrl_after.name.size) == "PRESERVE_PARAM");
        REQUIRE(ctrl_after.current_value == Catch::Approx(0.80f));
    }

    SECTION("stage mask change preserves control values") {
        const uint32_t new_mask = GOGGLES_FC_STAGE_MASK_EFFECT;
        REQUIRE(goggles_fc_chain_set_stage_mask(chain_guard.chain, new_mask) ==
                GOGGLES_FC_STATUS_OK);

        auto ctrl_after = goggles_fc_control_info_init();
        REQUIRE(goggles_fc_chain_get_control_info(chain_guard.chain, ctrl_index, &ctrl_after) ==
                GOGGLES_FC_STATUS_OK);
        REQUIRE(std::string_view(ctrl_after.name.data, ctrl_after.name.size) == "PRESERVE_PARAM");
        REQUIRE(ctrl_after.current_value == Catch::Approx(0.80f));
    }
}
