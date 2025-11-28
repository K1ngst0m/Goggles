// IPC socket handling for Vulkan layer (client side)

#pragma once

#include "capture/capture_protocol.hpp"

namespace goggles::capture {

// =============================================================================
// Layer Socket Client
// =============================================================================

class LayerSocketClient {
public:
    LayerSocketClient() = default;
    ~LayerSocketClient();

    // Non-copyable
    LayerSocketClient(const LayerSocketClient&) = delete;
    LayerSocketClient& operator=(const LayerSocketClient&) = delete;

    // Connect to Goggles app
    bool connect();
    void disconnect();
    bool is_connected() const { return socket_fd_ >= 0; }

    // Send texture data with DMA-BUF fd
    bool send_texture(const CaptureTextureData& data, int dmabuf_fd);

    // Check for control messages (non-blocking)
    bool poll_control(CaptureControl& control);

    // State
    bool is_capturing() const { return capturing_; }

private:
    int socket_fd_ = -1;
    bool capturing_ = false;
    int64_t last_connect_attempt_ = 0;
};

// Global socket client for the layer
LayerSocketClient& get_layer_socket();

} // namespace goggles::capture
