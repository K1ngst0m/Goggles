#pragma once

#include <array>
#include <cstdint>
#include <vulkan/vulkan.h>

namespace goggles::capture {

// NOLINTNEXTLINE(modernize-avoid-c-arrays) - abstract socket path requires C array
constexpr const char CAPTURE_SOCKET_PATH[] = "\0goggles/vkcapture";
constexpr size_t CAPTURE_SOCKET_PATH_LEN = sizeof(CAPTURE_SOCKET_PATH) - 1;

// NOLINTNEXTLINE(performance-enum-size) - uint32_t required for wire format stability
enum class CaptureMessageType : uint32_t {
    client_hello = 1,
    texture_data = 2,
    control = 3,
    semaphore_init = 4,
    frame_metadata = 5,
};

struct CaptureClientHello {
    CaptureMessageType type = CaptureMessageType::client_hello;
    uint32_t version = 1;
    std::array<char, 64> exe_name{}; // Executable name for identification
};

static_assert(sizeof(CaptureClientHello) == 72);

struct CaptureTextureData {
    CaptureMessageType type = CaptureMessageType::texture_data;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t stride = 0;
    uint32_t offset = 0;
    uint64_t modifier = 0;
};

static_assert(sizeof(CaptureTextureData) == 32);

struct CaptureControl {
    CaptureMessageType type = CaptureMessageType::control;
    uint32_t capturing = 0;
    std::array<uint32_t, 2> reserved{};
};

static_assert(sizeof(CaptureControl) == 16);

struct CaptureSemaphoreInit {
    CaptureMessageType type = CaptureMessageType::semaphore_init;
    uint32_t version = 1;
    uint64_t initial_value = 0;
    // Two FDs via SCM_RIGHTS: [frame_ready_fd, frame_consumed_fd]
};

static_assert(sizeof(CaptureSemaphoreInit) == 16);

struct CaptureFrameMetadata {
    CaptureMessageType type = CaptureMessageType::frame_metadata;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t stride = 0;
    uint32_t offset = 0;
    uint64_t modifier = 0;
    uint64_t frame_number = 0;
};

static_assert(sizeof(CaptureFrameMetadata) == 40);

} // namespace goggles::capture
