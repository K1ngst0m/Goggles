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
    CLIENT_HELLO = 1,
    TEXTURE_DATA = 2,
    CONTROL = 3,
};

struct CaptureClientHello {
    CaptureMessageType type = CaptureMessageType::CLIENT_HELLO;
    uint32_t version = 1;
    std::array<char, 64> exe_name{};  // Executable name for identification
};

static_assert(sizeof(CaptureClientHello) == 72);

struct CaptureTextureData {
    CaptureMessageType type = CaptureMessageType::TEXTURE_DATA;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t stride = 0;
    uint32_t offset = 0;
    uint64_t modifier = 0;
};

static_assert(sizeof(CaptureTextureData) == 32);

struct CaptureControl {
    CaptureMessageType type = CaptureMessageType::CONTROL;
    uint32_t capturing = 0;
    std::array<uint32_t, 2> reserved{};
};

static_assert(sizeof(CaptureControl) == 16);

} // namespace goggles::capture
