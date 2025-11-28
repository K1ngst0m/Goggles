// Capture IPC protocol - shared between layer and app
// Based on obs-vkcapture protocol

#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace goggles::capture {

// Socket path (abstract namespace)
constexpr const char* CAPTURE_SOCKET_PATH = "\0goggles/vkcapture";
constexpr size_t CAPTURE_SOCKET_PATH_LEN = 18;  // Including null byte

// =============================================================================
// Message Types
// =============================================================================

enum class CaptureMessageType : uint32_t {
    CLIENT_HELLO = 1,    // Layer -> App: Initial connection
    TEXTURE_DATA = 2,    // Layer -> App: DMA-BUF fd + metadata
    CONTROL = 3,         // App -> Layer: Start/stop capture
};

// =============================================================================
// Client Hello (Layer -> App)
// =============================================================================

struct CaptureClientHello {
    CaptureMessageType type = CaptureMessageType::CLIENT_HELLO;
    uint32_t version = 1;
    char exe_name[64] = {};  // Executable name for identification
};

static_assert(sizeof(CaptureClientHello) == 72);

// =============================================================================
// Texture Data (Layer -> App) - sent with DMA-BUF fd via SCM_RIGHTS
// =============================================================================

struct CaptureTextureData {
    CaptureMessageType type = CaptureMessageType::TEXTURE_DATA;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t stride = 0;
    uint32_t offset = 0;
    uint64_t modifier = 0;  // DRM format modifier (0 = LINEAR)
    // DMA-BUF fd is sent via SCM_RIGHTS ancillary data
};

static_assert(sizeof(CaptureTextureData) == 32);

// =============================================================================
// Control (App -> Layer)
// =============================================================================

struct CaptureControl {
    CaptureMessageType type = CaptureMessageType::CONTROL;
    uint32_t capturing = 0;  // 1 = start, 0 = stop
    uint32_t reserved[2] = {};
};

static_assert(sizeof(CaptureControl) == 16);

} // namespace goggles::capture
