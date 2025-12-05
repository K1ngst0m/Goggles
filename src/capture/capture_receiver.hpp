#pragma once

#include "capture_protocol.hpp"
#include <cstdint>

namespace goggles {

struct CaptureFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t format = 0;
    int dmabuf_fd = -1;
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
    [[nodiscard]] bool has_frame() const { return m_frame.dmabuf_fd >= 0; }

private:
    bool accept_client();
    bool receive_message();
    void cleanup_frame();

    int m_listen_fd = -1;
    int m_client_fd = -1;
    CaptureFrame m_frame{};
    capture::CaptureTextureData m_last_texture{};
};

} // namespace goggles
