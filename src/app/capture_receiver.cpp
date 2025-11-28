// Capture receiver implementation

#include "capture_receiver.hpp"

#include <util/logging.hpp>

#include <cerrno>
#include <cmath>
#include <cstring>
#include <vector>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace goggles {

using namespace capture;

// =============================================================================
// Construction / Destruction
// =============================================================================

CaptureReceiver::CaptureReceiver() = default;

CaptureReceiver::~CaptureReceiver() {
    shutdown();
}

// =============================================================================
// Initialization
// =============================================================================

bool CaptureReceiver::init() {
    if (listen_fd_ >= 0) {
        return true;
    }

    // Create listening socket
    listen_fd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (listen_fd_ < 0) {
        GOGGLES_LOG_ERROR("Failed to create socket: {}", strerror(errno));
        return false;
    }

    // Bind to abstract socket
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, CAPTURE_SOCKET_PATH, CAPTURE_SOCKET_PATH_LEN);

    socklen_t addr_len = static_cast<socklen_t>(
        offsetof(sockaddr_un, sun_path) + CAPTURE_SOCKET_PATH_LEN);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0) {
        GOGGLES_LOG_ERROR("Failed to bind socket: {}", strerror(errno));
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (listen(listen_fd_, 1) < 0) {
        GOGGLES_LOG_ERROR("Failed to listen: {}", strerror(errno));
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    GOGGLES_LOG_INFO("Capture socket listening");
    return true;
}

void CaptureReceiver::shutdown() {
    cleanup_frame();

    if (client_fd_ >= 0) {
        close(client_fd_);
        client_fd_ = -1;
    }

    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
}

// =============================================================================
// Frame Polling
// =============================================================================

bool CaptureReceiver::poll_frame() {
    // Accept new client if none connected
    if (client_fd_ < 0) {
        accept_client();
    }

    // Try to receive frame
    if (client_fd_ >= 0) {
        return receive_message();
    }

    return false;
}

bool CaptureReceiver::accept_client() {
    if (listen_fd_ < 0) {
        return false;
    }

    int new_fd = accept4(listen_fd_, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (new_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            GOGGLES_LOG_ERROR("Accept failed: {}", strerror(errno));
        }
        return false;
    }

    // Close existing client
    if (client_fd_ >= 0) {
        close(client_fd_);
        cleanup_frame();
    }

    client_fd_ = new_fd;
    GOGGLES_LOG_INFO("Capture client connected");

    // Send control message to start capture
    CaptureControl ctrl{};
    ctrl.type = CaptureMessageType::CONTROL;
    ctrl.capturing = 1;
    send(client_fd_, &ctrl, sizeof(ctrl), MSG_NOSIGNAL);

    return true;
}

bool CaptureReceiver::receive_message() {
    if (client_fd_ < 0) {
        return false;
    }

    // Set up receive buffer for message + ancillary data
    char buf[128];
    char cmsg_buf[CMSG_SPACE(sizeof(int))];

    iovec iov{};
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);

    msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    ssize_t received = recvmsg(client_fd_, &msg, MSG_DONTWAIT);
    if (received <= 0) {
        if (received == 0) {
            GOGGLES_LOG_INFO("Capture client disconnected");
            close(client_fd_);
            client_fd_ = -1;
            cleanup_frame();
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            GOGGLES_LOG_ERROR("Receive failed: {}", strerror(errno));
            close(client_fd_);
            client_fd_ = -1;
            cleanup_frame();
        }
        return false;
    }

    // Check message type
    if (received < static_cast<ssize_t>(sizeof(CaptureMessageType))) {
        return false;
    }

    auto msg_type = *reinterpret_cast<CaptureMessageType*>(buf);

    if (msg_type == CaptureMessageType::CLIENT_HELLO) {
        if (received >= static_cast<ssize_t>(sizeof(CaptureClientHello))) {
            auto* hello = reinterpret_cast<CaptureClientHello*>(buf);
            GOGGLES_LOG_INFO("Capture client: {}", hello->exe_name);
        }
        return false;
    }

    if (msg_type == CaptureMessageType::TEXTURE_DATA) {
        if (received < static_cast<ssize_t>(sizeof(CaptureTextureData))) {
            return false;
        }

        auto* tex_data = reinterpret_cast<CaptureTextureData*>(buf);

        // Extract DMA-BUF fd from ancillary data
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
        GOGGLES_LOG_DEBUG("Received texture: {}x{}, fd={}, stride={}",
                         tex_data->width, tex_data->height, new_fd, tex_data->stride);

        // Check if texture changed
        bool texture_changed = (tex_data->width != last_texture_.width ||
                                tex_data->height != last_texture_.height ||
                                tex_data->stride != last_texture_.stride);

        if (texture_changed) {
            // Unmap old frame
            unmap_frame();

            // Close old fd if different
            if (frame_.dmabuf_fd >= 0 && frame_.dmabuf_fd != new_fd) {
                close(frame_.dmabuf_fd);
            }

            frame_.dmabuf_fd = new_fd;
            frame_.width = tex_data->width;
            frame_.height = tex_data->height;
            frame_.stride = tex_data->stride;
            frame_.data_size = static_cast<size_t>(tex_data->stride) * tex_data->height;

            // mmap the DMA-BUF
            frame_.data = mmap(nullptr, frame_.data_size, PROT_READ, MAP_SHARED,
                               frame_.dmabuf_fd, 0);
            if (frame_.data == MAP_FAILED) {
                GOGGLES_LOG_ERROR("mmap failed: {}", strerror(errno));
                frame_.data = nullptr;
                close(frame_.dmabuf_fd);
                frame_.dmabuf_fd = -1;
                return false;
            }

            last_texture_ = *tex_data;
            GOGGLES_LOG_INFO("Capture texture: {}x{}, stride={}", 
                            frame_.width, frame_.height, frame_.stride);
        } else {
            // Same texture, just close the duplicate fd
            close(new_fd);
        }

        // Sync DMA-BUF before reading
        if (frame_.data != nullptr) {
            dma_buf_sync sync{};
            sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ;
            ioctl(frame_.dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync);
        }

        return frame_.data != nullptr;
    }

    return false;
}

// =============================================================================
// SDL Texture Update
// =============================================================================

SDL_Texture* CaptureReceiver::update_texture(SDL_Renderer* renderer,
                                              SDL_Texture* existing) {
    if (frame_.data == nullptr || frame_.width == 0 || frame_.height == 0) {
        return existing;
    }

    // Create texture - use BGRA32 for now, with proper conversion
    if (existing == nullptr) {
        existing = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     static_cast<int>(frame_.width),
                                     static_cast<int>(frame_.height));
        if (existing == nullptr) {
            GOGGLES_LOG_ERROR("Failed to create texture: {}", SDL_GetError());
            return nullptr;
        }
        // Disable alpha blending - we only support VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        // For non-opaque windows, the layer logs a warning but we still treat as opaque
        SDL_SetTextureBlendMode(existing, SDL_BLENDMODE_NONE);
    }

    // Debug: dump raw 10-bit pixel values
    static bool dumped = false;
    if (!dumped && frame_.data != nullptr) {
        auto* raw = static_cast<uint32_t*>(frame_.data);
        uint32_t pixel0 = raw[0];
        uint32_t r = (pixel0 >> 0) & 0x3FF;
        uint32_t g = (pixel0 >> 10) & 0x3FF;
        uint32_t b = (pixel0 >> 20) & 0x3FF;
        uint32_t a = (pixel0 >> 30) & 0x3;
        GOGGLES_LOG_INFO("Raw pixel[0] = 0x{:08X}, R10={} G10={} B10={} A2={}",
                        pixel0, r, g, b, a);
        GOGGLES_LOG_INFO("Converted to 8-bit: R={} G={} B={}",
                        r * 255 / 1023, g * 255 / 1023, b * 255 / 1023);
        dumped = true;
    }

    // Convert A2B10G10R10 to BGRA8
    static std::vector<uint8_t> converted;
    converted.resize(frame_.width * frame_.height * 4);

    for (uint32_t y = 0; y < frame_.height; ++y) {
        auto* src_row = reinterpret_cast<uint32_t*>(
            static_cast<uint8_t*>(frame_.data) + y * frame_.stride);
        for (uint32_t x = 0; x < frame_.width; ++x) {
            uint32_t p = src_row[x];
            uint32_t r = (p >> 0) & 0x3FF;
            uint32_t g = (p >> 10) & 0x3FF;
            uint32_t b = (p >> 20) & 0x3FF;
            size_t dst_idx = (y * frame_.width + x) * 4;
            uint32_t a = (p >> 30) & 0x3;
            converted[dst_idx + 0] = static_cast<uint8_t>(b * 255 / 1023);
            converted[dst_idx + 1] = static_cast<uint8_t>(g * 255 / 1023);
            converted[dst_idx + 2] = static_cast<uint8_t>(r * 255 / 1023);
            converted[dst_idx + 3] = static_cast<uint8_t>(a * 85);  // Preserve actual alpha
        }
    }

    SDL_UpdateTexture(existing, nullptr, converted.data(),
                      static_cast<int>(frame_.width * 4));

    // End DMA-BUF sync
    if (frame_.dmabuf_fd >= 0) {
        dma_buf_sync sync{};
        sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;
        ioctl(frame_.dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync);
    }

    return existing;
}

// =============================================================================
// Cleanup
// =============================================================================

void CaptureReceiver::cleanup_frame() {
    unmap_frame();

    if (frame_.dmabuf_fd >= 0) {
        close(frame_.dmabuf_fd);
        frame_.dmabuf_fd = -1;
    }

    last_texture_ = {};
}

void CaptureReceiver::unmap_frame() {
    if (frame_.data != nullptr && frame_.data != MAP_FAILED) {
        munmap(frame_.data, frame_.data_size);
        frame_.data = nullptr;
        frame_.data_size = 0;
    }
}

} // namespace goggles
