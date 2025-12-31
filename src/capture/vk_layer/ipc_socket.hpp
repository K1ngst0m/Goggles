#pragma once

#include "capture/capture_protocol.hpp"

#include <mutex>

namespace goggles::capture {

class LayerSocketClient {
public:
    LayerSocketClient() = default;
    ~LayerSocketClient();

    LayerSocketClient(const LayerSocketClient&) = delete;
    LayerSocketClient& operator=(const LayerSocketClient&) = delete;

    bool connect();
    void disconnect();
    bool is_connected() const;
    bool send_texture(const CaptureTextureData& data, int dmabuf_fd);
    bool send_semaphores(int frame_ready_fd, int frame_consumed_fd);
    bool send_texture_with_fd(const CaptureFrameMetadata& metadata, int dmabuf_fd);
    bool send_frame_metadata(const CaptureFrameMetadata& metadata);
    bool poll_control(CaptureControl& control);
    bool request_display_config(CaptureInputDisplayReady& response);
    bool is_capturing() const;

private:
    mutable std::mutex mutex_;
    int socket_fd_ = -1;
    bool capturing_ = false;
    int64_t last_connect_attempt_ = 0;
};

LayerSocketClient& get_layer_socket();

} // namespace goggles::capture
