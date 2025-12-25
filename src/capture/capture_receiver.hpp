#pragma once

#include "capture_protocol.hpp"

#include <cstdint>
#include <util/unique_fd.hpp>

namespace goggles {

struct CaptureFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t format = 0;
    util::UniqueFd dmabuf_fd;
    uint64_t modifier = 0;
    uint64_t frame_number = 0;
};

class CaptureReceiver {
public:
    CaptureReceiver();
    ~CaptureReceiver();

    CaptureReceiver(const CaptureReceiver&) = delete;
    CaptureReceiver& operator=(const CaptureReceiver&) = delete;

    bool init();
    void shutdown();
    bool poll_frame();

    [[nodiscard]] const CaptureFrame& get_frame() const { return m_frame; }
    [[nodiscard]] bool is_connected() const { return m_client_fd >= 0; }
    [[nodiscard]] bool has_frame() const { return m_frame.dmabuf_fd.valid(); }

    [[nodiscard]] int get_frame_ready_fd() const { return m_frame_ready_fd; }
    [[nodiscard]] int get_frame_consumed_fd() const { return m_frame_consumed_fd; }
    [[nodiscard]] bool has_sync_semaphores() const {
        return m_frame_ready_fd >= 0 && m_frame_consumed_fd >= 0;
    }
    [[nodiscard]] bool semaphores_updated() const { return m_semaphores_updated; }
    void clear_semaphores_updated() { m_semaphores_updated = false; }
    void clear_sync_semaphores();

private:
    bool accept_client();
    bool receive_message();
    void cleanup_frame();

    int m_listen_fd = -1;
    int m_client_fd = -1;
    CaptureFrame m_frame{};
    capture::CaptureTextureData m_last_texture{};
    int m_frame_ready_fd = -1;
    int m_frame_consumed_fd = -1;
    bool m_semaphores_updated = false;
    bool m_awaiting_new_fd = false;
};

} // namespace goggles
