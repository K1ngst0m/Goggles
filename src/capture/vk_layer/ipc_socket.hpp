#pragma once

#include "capture/capture_protocol.hpp"

#include <atomic>
#include <mutex>

namespace goggles::capture {

/// @brief Virtual resolution request received from the viewer.
struct ResolutionRequest {
    bool pending = false;
    uint32_t width = 0;
    uint32_t height = 0;
};

/// @brief IPC client used by the Vulkan layer to send captured frames.
class LayerSocketClient {
public:
    LayerSocketClient() = default;
    ~LayerSocketClient();

    LayerSocketClient(const LayerSocketClient&) = delete;
    LayerSocketClient& operator=(const LayerSocketClient&) = delete;

    /// @brief Connects to the capture receiver socket.
    /// @return True if connected.
    bool connect();
    /// @brief Disconnects and closes any held socket resources.
    void disconnect();
    /// @brief Returns true if currently connected.
    bool is_connected() const;
    /// @brief Sends texture metadata and an exported DMA-BUF FD.
    /// @param data Texture metadata.
    /// @param dmabuf_fd Exported DMA-BUF file descriptor.
    /// @return True on success.
    bool send_texture(const CaptureTextureData& data, int dmabuf_fd);
    /// @brief Sends synchronization semaphore FDs via SCM_RIGHTS.
    /// @return True on success.
    bool send_semaphores(int frame_ready_fd, int frame_consumed_fd);
    /// @brief Sends per-frame metadata and an exported DMA-BUF FD.
    /// @return True on success.
    bool send_texture_with_fd(const CaptureFrameMetadata& metadata, int dmabuf_fd);
    /// @brief Sends per-frame metadata without transferring any FDs.
    /// @return True on success.
    bool send_frame_metadata(const CaptureFrameMetadata& metadata);
    /// @brief Polls for control messages from the receiver.
    /// @param control Output control message.
    /// @return True on success.
    bool poll_control(CaptureControl& control);
    /// @brief Returns the last known capture-enabled state.
    bool is_capturing() const;
    /// @brief Consumes a pending resolution request, if any.
    ResolutionRequest consume_resolution_request();

private:
    mutable std::mutex mutex_;
    int socket_fd_ = -1;
    std::atomic<bool> capturing_{false};
    int64_t last_connect_attempt_ = 0;
    std::atomic<bool> res_pending_{false};
    std::atomic<uint32_t> res_width_{0};
    std::atomic<uint32_t> res_height_{0};
};

/// @brief Returns the process-wide layer socket client instance.
LayerSocketClient& get_layer_socket();

} // namespace goggles::capture
