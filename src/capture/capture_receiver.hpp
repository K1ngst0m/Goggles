#pragma once

#include "capture_protocol.hpp"

#include <cstdint>
#include <util/error.hpp>
#include <util/external_image.hpp>
#include <vector>

namespace goggles {

/// @brief Receives capture frames over the local IPC protocol.
class CaptureReceiver {
public:
    /// @brief Creates and starts a capture receiver.
    /// @return A receiver or an error.
    [[nodiscard]] static auto create() -> ResultPtr<CaptureReceiver>;

    ~CaptureReceiver();

    CaptureReceiver(const CaptureReceiver&) = delete;
    CaptureReceiver& operator=(const CaptureReceiver&) = delete;

    /// @brief Shuts down sockets and clears any held frame state.
    void shutdown();
    /// @brief Polls the socket and updates internal state if a new frame arrives.
    /// @return True if progress was made, false if disconnected or no data.
    bool poll_frame();
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    /// @brief Requests the capture client to change resolution.
    /// @param width Requested width in pixels.
    /// @param height Requested height in pixels.
    void request_resolution(uint32_t width, uint32_t height);

    /// @brief Returns the most recent frame metadata.
    [[nodiscard]] const util::ExternalImageFrame& get_frame() const { return m_frame; }
    /// @brief Returns true if a client is connected.
    [[nodiscard]] bool is_connected() const { return m_client_fd >= 0; }
    /// @brief Returns true if a frame DMA-BUF FD is available.
    [[nodiscard]] bool has_frame() const { return m_frame.image.handle.valid(); }

    /// @brief Returns the "frame ready" semaphore FD, or `-1` if unavailable.
    [[nodiscard]] int get_frame_ready_fd() const { return m_frame_ready_fd; }
    /// @brief Returns the "frame consumed" semaphore FD, or `-1` if unavailable.
    [[nodiscard]] int get_frame_consumed_fd() const { return m_frame_consumed_fd; }
    /// @brief Returns true if both sync semaphore FDs are available.
    [[nodiscard]] bool has_sync_semaphores() const {
        return m_frame_ready_fd >= 0 && m_frame_consumed_fd >= 0;
    }
    /// @brief Returns true if sync semaphore FDs changed since last clear.
    [[nodiscard]] bool semaphores_updated() const { return m_semaphores_updated; }
    /// @brief Clears the "semaphores updated" flag.
    void clear_semaphores_updated() { m_semaphores_updated = false; }
    /// @brief Clears and closes any held sync semaphore FDs.
    void clear_sync_semaphores();

private:
    CaptureReceiver() = default;

    bool accept_client();
    bool receive_message();
    void cleanup_frame();

    bool process_message(const char* data, size_t len, const std::vector<int>& fds,
                         size_t& fd_index);

    int m_listen_fd = -1;
    int m_client_fd = -1;
    util::ExternalImageFrame m_frame{};
    capture::CaptureTextureData m_last_texture{};
    int m_frame_ready_fd = -1;
    int m_frame_consumed_fd = -1;
    bool m_semaphores_updated = false;

    std::vector<char> m_recv_buf;
};

} // namespace goggles
