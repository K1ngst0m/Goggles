#include "capture_receiver.hpp"

#include <util/logging.hpp>

#include <array>
#include <cerrno>
#include <cstring>
#include <utility>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace goggles {

using namespace capture;

CaptureReceiver::CaptureReceiver() = default;

CaptureReceiver::~CaptureReceiver() {
    shutdown();
}

bool CaptureReceiver::init() {
    if (m_listen_fd >= 0) {
        return true;
    }

    m_listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (m_listen_fd < 0) {
        GOGGLES_LOG_ERROR("Failed to create socket: {}", strerror(errno));
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, CAPTURE_SOCKET_PATH, CAPTURE_SOCKET_PATH_LEN);

    auto addr_len = static_cast<socklen_t>(
        offsetof(sockaddr_un, sun_path) + CAPTURE_SOCKET_PATH_LEN);

    if (bind(m_listen_fd, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0) {
        GOGGLES_LOG_ERROR("Failed to bind socket: {}", strerror(errno));
        close(m_listen_fd);
        m_listen_fd = -1;
        return false;
    }

    if (listen(m_listen_fd, 1) < 0) {
        GOGGLES_LOG_ERROR("Failed to listen: {}", strerror(errno));
        close(m_listen_fd);
        m_listen_fd = -1;
        return false;
    }

    GOGGLES_LOG_INFO("Capture socket listening");
    return true;
}

void CaptureReceiver::shutdown() {
    cleanup_frame();

    if (m_client_fd >= 0) {
        close(m_client_fd);
        m_client_fd = -1;
    }

    if (m_listen_fd >= 0) {
        close(m_listen_fd);
        m_listen_fd = -1;
    }
}

bool CaptureReceiver::poll_frame() {
    if (m_client_fd < 0) {
        accept_client();
    }

    if (m_client_fd >= 0) {
        return receive_message();
    }

    return false;
}

bool CaptureReceiver::accept_client() {
    if (m_listen_fd < 0) {
        return false;
    }

    int new_fd = accept4(m_listen_fd, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (new_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            GOGGLES_LOG_ERROR("Accept failed: {}", strerror(errno));
        }
        return false;
    }

    if (m_client_fd >= 0) {
        close(m_client_fd);
        cleanup_frame();
    }

    m_client_fd = new_fd;
    GOGGLES_LOG_INFO("Capture client connected");

    CaptureControl ctrl{};
    ctrl.type = CaptureMessageType::CONTROL;
    ctrl.capturing = 1;
    send(m_client_fd, &ctrl, sizeof(ctrl), MSG_NOSIGNAL);

    return true;
}

bool CaptureReceiver::receive_message() {
    if (m_client_fd < 0) {
        return false;
    }

    std::array<char, 128> buf{};
    std::array<char, CMSG_SPACE(sizeof(int))> cmsg_buf{};

    iovec iov{};
    iov.iov_base = buf.data();
    iov.iov_len = buf.size();

    msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf.data();
    msg.msg_controllen = cmsg_buf.size();

    ssize_t received = recvmsg(m_client_fd, &msg, MSG_DONTWAIT);
    if (received <= 0) {
        if (received == 0) {
            GOGGLES_LOG_INFO("Capture client disconnected");
            close(m_client_fd);
            m_client_fd = -1;
            cleanup_frame();
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            GOGGLES_LOG_ERROR("Receive failed: {}", strerror(errno));
            close(m_client_fd);
            m_client_fd = -1;
            cleanup_frame();
        }
        return false;
    }

    if (std::cmp_less(received, sizeof(CaptureMessageType))) {
        return false;
    }

    auto msg_type = *reinterpret_cast<CaptureMessageType*>(buf.data());

    if (msg_type == CaptureMessageType::CLIENT_HELLO) {
        if (std::cmp_greater_equal(received, sizeof(CaptureClientHello))) {
            auto* hello = reinterpret_cast<CaptureClientHello*>(buf.data());
            GOGGLES_LOG_INFO("Capture client: {}", hello->exe_name.data());
        }
        return false;
    }

    if (msg_type == CaptureMessageType::TEXTURE_DATA) {
        if (std::cmp_less(received, sizeof(CaptureTextureData))) {
            return false;
        }

        auto* tex_data = reinterpret_cast<CaptureTextureData*>(buf.data());

        int new_fd = -1;
        for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr;
             cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                std::memcpy(&new_fd, CMSG_DATA(cmsg), sizeof(int));
                break;
            }
        }

        if (new_fd < 0) {
            GOGGLES_LOG_WARN("TEXTURE_DATA received but no fd in ancillary data");
            return false;
        }

        GOGGLES_LOG_DEBUG("Received texture: {}x{}, fd={}, stride={}, format={}",
                         tex_data->width, tex_data->height, new_fd, 
                         tex_data->stride, static_cast<int>(tex_data->format));

        bool texture_changed = (tex_data->width != m_last_texture.width ||
                                tex_data->height != m_last_texture.height ||
                                tex_data->format != m_last_texture.format);

        if (texture_changed) {
            if (m_frame.dmabuf_fd >= 0) {
                close(m_frame.dmabuf_fd);
            }

            m_frame.dmabuf_fd = new_fd;
            m_frame.width = tex_data->width;
            m_frame.height = tex_data->height;
            m_frame.stride = tex_data->stride;
            m_frame.format = static_cast<uint32_t>(tex_data->format);
            m_last_texture = *tex_data;

            GOGGLES_LOG_INFO("Capture texture: {}x{}, format={}", 
                            m_frame.width, m_frame.height, m_frame.format);
        } else {
            close(new_fd);
        }

        return m_frame.dmabuf_fd >= 0;
    }

    return false;
}

void CaptureReceiver::cleanup_frame() {
    if (m_frame.dmabuf_fd >= 0) {
        close(m_frame.dmabuf_fd);
        m_frame.dmabuf_fd = -1;
    }
    m_frame = {};
    m_last_texture = {};
}

} // namespace goggles
