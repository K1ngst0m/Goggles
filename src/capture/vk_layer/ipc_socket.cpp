#include "ipc_socket.hpp"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <util/profiling.hpp>

#define LAYER_DEBUG(fmt, ...) fprintf(stderr, "[goggles-layer] " fmt "\n", ##__VA_ARGS__)

namespace goggles::capture {

LayerSocketClient& get_layer_socket() {
    static LayerSocketClient client;
    return client;
}

LayerSocketClient::~LayerSocketClient() {
    disconnect();
}

bool LayerSocketClient::connect() {
    GOGGLES_PROFILE_FUNCTION();
    std::lock_guard lock(mutex_);

    if (socket_fd_ >= 0) {
        return true;
    }

    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    if (now - last_connect_attempt_ < 1'000'000'000) {
        return false;
    }
    last_connect_attempt_ = now;

    socket_fd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (socket_fd_ < 0) {
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, CAPTURE_SOCKET_PATH, CAPTURE_SOCKET_PATH_LEN);

    socklen_t addr_len =
        static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + CAPTURE_SOCKET_PATH_LEN);

    if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    CaptureClientHello hello{};
    hello.type = CaptureMessageType::client_hello;
    hello.version = 1;

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
    std::lock_guard lock(mutex_);

    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    capturing_ = false;
}

bool LayerSocketClient::is_connected() const {
    std::lock_guard lock(mutex_);
    return socket_fd_ >= 0;
}

bool LayerSocketClient::is_capturing() const {
    std::lock_guard lock(mutex_);
    return capturing_;
}

bool LayerSocketClient::send_texture(const CaptureTextureData& data, int dmabuf_fd) {
    GOGGLES_PROFILE_FUNCTION();
    std::lock_guard lock(mutex_);

    if (socket_fd_ < 0 || dmabuf_fd < 0) {
        return false;
    }

    msghdr msg{};
    iovec iov{};
    iov.iov_base = const_cast<CaptureTextureData*>(&data);
    iov.iov_len = sizeof(data);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

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
            close(socket_fd_);
            socket_fd_ = -1;
            capturing_ = false;
        }
        return false;
    }

    return sent == sizeof(data);
}

bool LayerSocketClient::send_semaphores(int frame_ready_fd, int frame_consumed_fd) {
    GOGGLES_PROFILE_FUNCTION();
    std::lock_guard lock(mutex_);

    if (socket_fd_ < 0 || frame_ready_fd < 0 || frame_consumed_fd < 0) {
        return false;
    }

    CaptureSemaphoreInit init{};
    init.type = CaptureMessageType::semaphore_init;
    init.version = 1;
    init.initial_value = 0;

    msghdr msg{};
    iovec iov{};
    iov.iov_base = &init;
    iov.iov_len = sizeof(init);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    int fds[2] = {frame_ready_fd, frame_consumed_fd};
    char cmsg_buf[CMSG_SPACE(sizeof(fds))];
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fds));
    std::memcpy(CMSG_DATA(cmsg), fds, sizeof(fds));

    ssize_t sent = sendmsg(socket_fd_, &msg, MSG_NOSIGNAL);
    if (sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            close(socket_fd_);
            socket_fd_ = -1;
            capturing_ = false;
        }
        return false;
    }

    return sent == sizeof(init);
}

bool LayerSocketClient::send_texture_with_fd(const CaptureFrameMetadata& metadata, int dmabuf_fd) {
    GOGGLES_PROFILE_FUNCTION();
    std::lock_guard lock(mutex_);

    if (socket_fd_ < 0 || dmabuf_fd < 0) {
        return false;
    }

    msghdr msg{};
    iovec iov{};
    iov.iov_base = const_cast<CaptureFrameMetadata*>(&metadata);
    iov.iov_len = sizeof(metadata);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

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
            close(socket_fd_);
            socket_fd_ = -1;
            capturing_ = false;
        }
        return false;
    }

    return sent == sizeof(metadata);
}

bool LayerSocketClient::send_frame_metadata(const CaptureFrameMetadata& metadata) {
    GOGGLES_PROFILE_FUNCTION();
    std::lock_guard lock(mutex_);

    if (socket_fd_ < 0) {
        return false;
    }

    ssize_t sent = send(socket_fd_, &metadata, sizeof(metadata), MSG_NOSIGNAL);
    if (sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            close(socket_fd_);
            socket_fd_ = -1;
            capturing_ = false;
        }
        return false;
    }

    return sent == sizeof(metadata);
}

bool LayerSocketClient::poll_control(CaptureControl& control) {
    GOGGLES_PROFILE_FUNCTION();
    int fd;
    {
        std::lock_guard lock(mutex_);
        fd = socket_fd_;
    }

    if (fd < 0) {
        return false;
    }

    ssize_t received = recv(fd, &control, sizeof(control), MSG_DONTWAIT);

    if (received == sizeof(control) && control.type == CaptureMessageType::control) {
        std::lock_guard lock(mutex_);
        capturing_ = (control.capturing != 0);
        return true;
    }

    if (received == 0 || (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        std::lock_guard lock(mutex_);
        if (socket_fd_ == fd) {
            close(socket_fd_);
            socket_fd_ = -1;
            capturing_ = false;
        }
    }

    return false;
}

} // namespace goggles::capture
