// IPC socket implementation for Vulkan layer

#include "ipc_socket.hpp"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define LAYER_DEBUG(fmt, ...) fprintf(stderr, "[goggles-layer] " fmt "\n", ##__VA_ARGS__)

namespace goggles::capture {

// =============================================================================
// Global Instance
// =============================================================================

LayerSocketClient& get_layer_socket() {
    static LayerSocketClient client;
    return client;
}

// =============================================================================
// Implementation
// =============================================================================

LayerSocketClient::~LayerSocketClient() {
    disconnect();
}

bool LayerSocketClient::connect() {
    if (socket_fd_ >= 0) {
        return true;
    }

    // Rate-limit connection attempts (1 second)
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    if (now - last_connect_attempt_ < 1'000'000'000) {
        return false;
    }
    last_connect_attempt_ = now;

    // Create socket
    socket_fd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (socket_fd_ < 0) {
        return false;
    }

    // Connect to abstract socket
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, CAPTURE_SOCKET_PATH, CAPTURE_SOCKET_PATH_LEN);

    socklen_t addr_len = static_cast<socklen_t>(
        offsetof(sockaddr_un, sun_path) + CAPTURE_SOCKET_PATH_LEN);

    if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0) {
        LAYER_DEBUG("Socket connect failed: %s (is Goggles app running?)", strerror(errno));
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    // Send hello message
    CaptureClientHello hello{};
    hello.type = CaptureMessageType::CLIENT_HELLO;
    hello.version = 1;

    // Get executable name
    char exe_path[256];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        const char* name = strrchr(exe_path, '/');
        name = name ? name + 1 : exe_path;
        strncpy(hello.exe_name.data(), name, hello.exe_name.size() - 1);
    }

    ssize_t sent = send(socket_fd_, &hello, sizeof(hello), MSG_NOSIGNAL);
    if (sent != sizeof(hello)) {
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    return true;
}

void LayerSocketClient::disconnect() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    capturing_ = false;
}

bool LayerSocketClient::send_texture(const CaptureTextureData& data, int dmabuf_fd) {
    if (socket_fd_ < 0 || dmabuf_fd < 0) {
        return false;
    }

    // Set up message with ancillary data for fd passing
    msghdr msg{};
    iovec iov{};
    iov.iov_base = const_cast<CaptureTextureData*>(&data);
    iov.iov_len = sizeof(data);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // Control message buffer for SCM_RIGHTS
    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(cmsg), &dmabuf_fd, sizeof(int));

    ssize_t sent = sendmsg(socket_fd_, &msg, MSG_NOSIGNAL);
    if (sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            disconnect();
        }
        return false;
    }

    return sent == sizeof(data);
}

bool LayerSocketClient::poll_control(CaptureControl& control) {
    if (socket_fd_ < 0) {
        return false;
    }

    ssize_t received = recv(socket_fd_, &control, sizeof(control), MSG_DONTWAIT);
    if (received == sizeof(control) &&
        control.type == CaptureMessageType::CONTROL) {
        capturing_ = (control.capturing != 0);
        return true;
    }

    if (received == 0) {
        // Connection closed
        disconnect();
    } else if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        disconnect();
    }

    return false;
}

} // namespace goggles::capture
