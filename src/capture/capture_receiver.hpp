#pragma once

#include "capture_protocol.hpp"

#include <cstdint>
#include <util/error.hpp>
#include <util/unique_fd.hpp>
#include <vector>

namespace goggles {

struct CaptureFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t offset = 0;
    uint32_t format = 0;
    util::UniqueFd dmabuf_fd;
    uint64_t modifier = 0;
    uint64_t frame_number = 0;
};

class CaptureReceiver {
public:
    [[nodiscard]] static auto create() -> ResultPtr<CaptureReceiver>;

    ~CaptureReceiver();

    CaptureReceiver(const CaptureReceiver&) = delete;
    CaptureReceiver& operator=(const CaptureReceiver&) = delete;

    void shutdown();
    bool poll_frame();
    void request_resolution(uint32_t width, uint32_t height);

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
    CaptureReceiver() = default;

    bool accept_client();
    bool receive_message();
    void cleanup_frame();

    bool process_message(const char* data, size_t len, const std::vector<int>& fds,
                         size_t& fd_index);

    int m_listen_fd = -1;
    int m_client_fd = -1;
    CaptureFrame m_frame{};
    capture::CaptureTextureData m_last_texture{};
    int m_frame_ready_fd = -1;
    int m_frame_consumed_fd = -1;
    bool m_semaphores_updated = false;

    std::vector<char> m_recv_buf;
};

} // namespace goggles
