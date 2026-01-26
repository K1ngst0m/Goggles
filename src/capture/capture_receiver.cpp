#include "capture_receiver.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles {

using namespace capture;

CaptureReceiver::~CaptureReceiver() {
    shutdown();
}

auto CaptureReceiver::create() -> ResultPtr<CaptureReceiver> {
    auto receiver = std::unique_ptr<CaptureReceiver>(new CaptureReceiver());

    receiver->m_listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (receiver->m_listen_fd < 0) {
        return make_result_ptr_error<CaptureReceiver>(ErrorCode::capture_init_failed,
                                                      std::string("Failed to create socket: ") +
                                                          strerror(errno));
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, CAPTURE_SOCKET_PATH, CAPTURE_SOCKET_PATH_LEN);

    auto addr_len =
        static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + CAPTURE_SOCKET_PATH_LEN);

    if (bind(receiver->m_listen_fd, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0) {
        std::string error_msg;
        if (errno == EADDRINUSE) {
            error_msg = "Capture socket already in use (another instance running?)";
        } else {
            error_msg = std::string("Failed to bind socket: ") + strerror(errno);
        }
        close(receiver->m_listen_fd);
        receiver->m_listen_fd = -1;
        return make_result_ptr_error<CaptureReceiver>(ErrorCode::capture_init_failed, error_msg);
    }

    if (listen(receiver->m_listen_fd, 1) < 0) {
        close(receiver->m_listen_fd);
        receiver->m_listen_fd = -1;
        return make_result_ptr_error<CaptureReceiver>(
            ErrorCode::capture_init_failed, std::string("Failed to listen: ") + strerror(errno));
    }

    GOGGLES_LOG_INFO("Capture socket listening");
    return make_result_ptr(std::move(receiver));
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
    GOGGLES_PROFILE_FUNCTION();

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

    // Normal frame capture connection
    if (m_client_fd >= 0) {
        GOGGLES_LOG_WARN("Rejecting new client: already connected");
        close(new_fd);
        return false;
    }

    m_client_fd = new_fd;
    GOGGLES_LOG_INFO("Capture client connected");

    CaptureControl ctrl{};
    ctrl.type = CaptureMessageType::control;
    ctrl.flags = CAPTURE_CONTROL_CAPTURING;

    size_t total_sent = 0;
    while (total_sent < sizeof(ctrl)) {
        ssize_t sent = send(m_client_fd, reinterpret_cast<const char*>(&ctrl) + total_sent,
                            sizeof(ctrl) - total_sent, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                pollfd pfd{m_client_fd, POLLOUT, 0};
                poll(&pfd, 1, 100);
                continue;
            }
            GOGGLES_LOG_ERROR("Failed to send initial control: {}", strerror(errno));
            close(m_client_fd);
            m_client_fd = -1;
            return false;
        }
        total_sent += static_cast<size_t>(sent);
    }

    return true;
}

bool CaptureReceiver::receive_message() {
    if (m_client_fd < 0) {
        return false;
    }

    std::array<char, 256> buf{};
    std::array<char, CMSG_SPACE(4 * sizeof(int))> cmsg_buf{};

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

    // Extract all FDs from ancillary data upfront
    std::vector<int> received_fds;
    for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            size_t fd_count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            for (size_t i = 0; i < fd_count; ++i) {
                int fd;
                std::memcpy(&fd, CMSG_DATA(cmsg) + i * sizeof(int), sizeof(int));
                received_fds.push_back(fd);
            }
        }
    }

    // Append to persistent buffer
    m_recv_buf.insert(m_recv_buf.end(), buf.data(), buf.data() + received);

    // Process all complete messages
    bool got_frame = false;
    size_t fd_index = 0;

    while (m_recv_buf.size() >= sizeof(CaptureMessageType)) {
        auto msg_type = *reinterpret_cast<CaptureMessageType*>(m_recv_buf.data());
        size_t msg_size = 0;

        switch (msg_type) {
        case CaptureMessageType::client_hello:
            msg_size = sizeof(CaptureClientHello);
            break;
        case CaptureMessageType::texture_data:
            msg_size = sizeof(CaptureTextureData);
            break;
        case CaptureMessageType::control:
            msg_size = sizeof(CaptureControl);
            break;
        case CaptureMessageType::semaphore_init:
            msg_size = sizeof(CaptureSemaphoreInit);
            break;
        case CaptureMessageType::frame_metadata:
            msg_size = sizeof(CaptureFrameMetadata);
            break;
        default:
            GOGGLES_LOG_ERROR("Unknown message type {}, disconnecting client",
                              static_cast<uint32_t>(msg_type));
            for (size_t i = fd_index; i < received_fds.size(); ++i) {
                close(received_fds[i]);
            }
            close(m_client_fd);
            m_client_fd = -1;
            cleanup_frame();
            return false;
        }

        if (m_recv_buf.size() < msg_size) {
            break;
        }

        if (process_message(m_recv_buf.data(), msg_size, received_fds, fd_index)) {
            got_frame = true;
        }

        m_recv_buf.erase(m_recv_buf.begin(), m_recv_buf.begin() + static_cast<ptrdiff_t>(msg_size));
    }

    // Close unused FDs
    for (size_t i = fd_index; i < received_fds.size(); ++i) {
        close(received_fds[i]);
    }

    return got_frame;
}

bool CaptureReceiver::process_message(const char* data, size_t len, const std::vector<int>& fds,
                                      size_t& fd_index) {
    auto msg_type = *reinterpret_cast<const CaptureMessageType*>(data);

    if (msg_type == CaptureMessageType::client_hello) {
        if (len >= sizeof(CaptureClientHello)) {
            auto* hello = reinterpret_cast<const CaptureClientHello*>(data);
            std::string_view exe_name(hello->exe_name.data(),
                                      strnlen(hello->exe_name.data(), hello->exe_name.size()));
            GOGGLES_LOG_INFO("Capture client: {}", exe_name);
        }
        return false;
    }

    if (msg_type == CaptureMessageType::texture_data) {
        if (len < sizeof(CaptureTextureData)) {
            return false;
        }

        auto* tex_data = reinterpret_cast<const CaptureTextureData*>(data);

        if (fd_index >= fds.size()) {
            GOGGLES_LOG_WARN("TEXTURE_DATA received but no fd available");
            return false;
        }
        int new_fd = fds[fd_index++];

        bool texture_changed =
            (tex_data->width != m_last_texture.width || tex_data->height != m_last_texture.height ||
             tex_data->format != m_last_texture.format ||
             tex_data->offset != m_last_texture.offset ||
             tex_data->modifier != m_last_texture.modifier);

        m_frame.image.handle = util::UniqueFd{new_fd};
        m_frame.image.width = tex_data->width;
        m_frame.image.height = tex_data->height;
        m_frame.image.stride = tex_data->stride;
        m_frame.image.offset = tex_data->offset;
        m_frame.image.format = static_cast<vk::Format>(tex_data->format);
        m_frame.image.modifier = tex_data->modifier;
        m_frame.image.handle_type = util::ExternalHandleType::dmabuf;
        m_last_texture = *tex_data;

        if (texture_changed) {
            GOGGLES_LOG_INFO("Capture texture: {}x{}, format={}, modifier=0x{:x}",
                             m_frame.image.width, m_frame.image.height,
                             static_cast<uint32_t>(m_frame.image.format), m_frame.image.modifier);
        }

        return m_frame.image.handle.valid();
    }

    if (msg_type == CaptureMessageType::semaphore_init) {
        if (len < sizeof(CaptureSemaphoreInit)) {
            return false;
        }

        if (fd_index + 2 > fds.size()) {
            GOGGLES_LOG_WARN("semaphore_init: need 2 fds, have {}", fds.size() - fd_index);
            return false;
        }

        int ready_fd = fds[fd_index++];
        int consumed_fd = fds[fd_index++];

        clear_sync_semaphores();
        m_frame.image.handle = util::UniqueFd{};
        m_frame_ready_fd = ready_fd;
        m_frame_consumed_fd = consumed_fd;
        m_semaphores_updated = true;
        GOGGLES_LOG_INFO("Received sync semaphores: ready_fd={}, consumed_fd={}", m_frame_ready_fd,
                         m_frame_consumed_fd);
        return false;
    }

    if (msg_type == CaptureMessageType::frame_metadata) {
        if (len < sizeof(CaptureFrameMetadata)) {
            return false;
        }

        auto* metadata = reinterpret_cast<const CaptureFrameMetadata*>(data);

        if (fd_index < fds.size()) {
            int new_fd = fds[fd_index++];
            m_frame.image.handle = util::UniqueFd{new_fd};
        }

        m_frame.image.width = metadata->width;
        m_frame.image.height = metadata->height;
        m_frame.image.stride = metadata->stride;
        m_frame.image.offset = metadata->offset;
        m_frame.image.format = static_cast<vk::Format>(metadata->format);
        m_frame.image.modifier = metadata->modifier;
        m_frame.image.handle_type = util::ExternalHandleType::dmabuf;
        m_frame.frame_number = metadata->frame_number;

        return m_frame.image.handle.valid();
    }

    return false;
}

void CaptureReceiver::clear_sync_semaphores() {
    if (m_frame_ready_fd >= 0) {
        close(m_frame_ready_fd);
        m_frame_ready_fd = -1;
    }
    if (m_frame_consumed_fd >= 0) {
        close(m_frame_consumed_fd);
        m_frame_consumed_fd = -1;
    }
}

void CaptureReceiver::cleanup_frame() {
    m_frame = {};
    m_last_texture = {};
    m_recv_buf.clear();
    clear_sync_semaphores();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void CaptureReceiver::request_resolution(uint32_t width, uint32_t height) {
    if (m_client_fd < 0) {
        return;
    }

    CaptureControl ctrl{};
    ctrl.type = CaptureMessageType::control;
    ctrl.flags = CAPTURE_CONTROL_CAPTURING | CAPTURE_CONTROL_RESOLUTION_REQUEST;
    ctrl.requested_width = width;
    ctrl.requested_height = height;
    send(m_client_fd, &ctrl, sizeof(ctrl), MSG_NOSIGNAL);
}

} // namespace goggles
