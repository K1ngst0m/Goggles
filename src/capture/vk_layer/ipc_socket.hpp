#pragma once

#include "capture/capture_protocol.hpp"

#include <mutex>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

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
    bool is_capturing() const;

    // Receive message with timeout (for config handshake)
    template<typename T>
    bool receive_with_timeout(T& msg, int timeout_ms);

private:
    mutable std::mutex mutex_;
    int socket_fd_ = -1;
    bool capturing_ = false;
    int64_t last_connect_attempt_ = 0;
};

LayerSocketClient& get_layer_socket();

// Template implementation
template<typename T>
bool LayerSocketClient::receive_with_timeout(T& msg, int timeout_ms) {
    std::lock_guard lock(mutex_);

    if (socket_fd_ < 0) {
        return false;
    }

    // Use poll() to wait for data with timeout
    pollfd pfd{};
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;

    int poll_result = poll(&pfd, 1, timeout_ms);
    if (poll_result <= 0) {
        // Timeout or error
        return false;
    }

    // Data available, read it
    ssize_t received = recv(socket_fd_, &msg, sizeof(T), MSG_DONTWAIT);
    return received == sizeof(T);
}

} // namespace goggles::capture
