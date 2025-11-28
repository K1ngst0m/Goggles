// Capture receiver implementation

#include "capture_receiver.hpp"

#include <util/logging.hpp>

#include <array>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <utility>
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
    if (m_listen_fd >= 0) {
        return true;
    }

    // Create listening socket
    m_listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (m_listen_fd < 0) {
        GOGGLES_LOG_ERROR("Failed to create socket: {}", strerror(errno));
        return false;
    }

    // Bind to abstract socket
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

// =============================================================================
// Frame Polling
// =============================================================================

bool CaptureReceiver::poll_frame() {
    // Accept new client if none connected
    if (m_client_fd < 0) {
        accept_client();
    }

    // Try to receive frame
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

    // Close existing client
    if (m_client_fd >= 0) {
        close(m_client_fd);
        cleanup_frame();
    }

    m_client_fd = new_fd;
    GOGGLES_LOG_INFO("Capture client connected");

    // Send control message to start capture
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

    // Set up receive buffer for message + ancillary data
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

    // Check message type
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
        bool texture_changed = (tex_data->width != m_last_texture.width ||
                                tex_data->height != m_last_texture.height ||
                                tex_data->stride != m_last_texture.stride);

        if (texture_changed) {
            // Unmap old frame
            unmap_frame();

            // Close old fd if different
            if (m_frame.dmabuf_fd >= 0 && m_frame.dmabuf_fd != new_fd) {
                close(m_frame.dmabuf_fd);
            }

            m_frame.dmabuf_fd = new_fd;
            m_frame.width = tex_data->width;
            m_frame.height = tex_data->height;
            m_frame.stride = tex_data->stride;
            m_frame.data_size = static_cast<size_t>(tex_data->stride) * tex_data->height;

            // mmap the DMA-BUF
            m_frame.data = mmap(nullptr, m_frame.data_size, PROT_READ, MAP_SHARED,
                               m_frame.dmabuf_fd, 0);
            if (m_frame.data == MAP_FAILED) {
                GOGGLES_LOG_ERROR("mmap failed: {}", strerror(errno));
                m_frame.data = nullptr;
                close(m_frame.dmabuf_fd);
                m_frame.dmabuf_fd = -1;
                return false;
            }

            m_last_texture = *tex_data;
            GOGGLES_LOG_INFO("Capture texture: {}x{}, stride={}", 
                            m_frame.width, m_frame.height, m_frame.stride);
        } else {
            // Same texture, just close the duplicate fd
            close(new_fd);
        }

        // Sync DMA-BUF before reading
        if (m_frame.data != nullptr) {
            dma_buf_sync sync{};
            sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ;
            ioctl(m_frame.dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync);
        }

        return m_frame.data != nullptr;
    }

    return false;
}

// =============================================================================
// SDL Texture Update
// =============================================================================

SDL_Texture* CaptureReceiver::update_texture(SDL_Renderer* renderer,
                                              SDL_Texture* existing) {
    if (m_frame.data == nullptr || m_frame.width == 0 || m_frame.height == 0) {
        return existing;
    }

    // Create texture - use BGRA32 for now, with proper conversion
    if (existing == nullptr) {
        existing = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     static_cast<int>(m_frame.width),
                                     static_cast<int>(m_frame.height));
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
    if (!dumped && m_frame.data != nullptr) {
        auto* raw = static_cast<uint32_t*>(m_frame.data);
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
    converted.resize(static_cast<size_t>(m_frame.width) * m_frame.height * 4);

    for (uint32_t y = 0; y < m_frame.height; ++y) {
        auto* src_row = reinterpret_cast<uint32_t*>(
            static_cast<uint8_t*>(m_frame.data) + static_cast<size_t>(y) * m_frame.stride);
        for (uint32_t x = 0; x < m_frame.width; ++x) {
            uint32_t p = src_row[x];
            uint32_t r = (p >> 0) & 0x3FF;
            uint32_t g = (p >> 10) & 0x3FF;
            uint32_t b = (p >> 20) & 0x3FF;
            size_t dst_idx = (static_cast<size_t>(y) * m_frame.width + x) * 4;
            uint32_t a = (p >> 30) & 0x3;
            converted[dst_idx + 0] = static_cast<uint8_t>(b * 255 / 1023);
            converted[dst_idx + 1] = static_cast<uint8_t>(g * 255 / 1023);
            converted[dst_idx + 2] = static_cast<uint8_t>(r * 255 / 1023);
            converted[dst_idx + 3] = static_cast<uint8_t>(a * 85);  // Preserve actual alpha
        }
    }

    SDL_UpdateTexture(existing, nullptr, converted.data(),
                      static_cast<int>(m_frame.width * 4));

    // End DMA-BUF sync
    if (m_frame.dmabuf_fd >= 0) {
        dma_buf_sync sync{};
        sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;
        ioctl(m_frame.dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync);
    }

    return existing;
}

// =============================================================================
// Cleanup
// =============================================================================

void CaptureReceiver::cleanup_frame() {
    unmap_frame();

    if (m_frame.dmabuf_fd >= 0) {
        close(m_frame.dmabuf_fd);
        m_frame.dmabuf_fd = -1;
    }

    m_last_texture = {};
}

void CaptureReceiver::unmap_frame() {
    if (m_frame.data != nullptr && m_frame.data != MAP_FAILED) {
        munmap(m_frame.data, m_frame.data_size);
        m_frame.data = nullptr;
        m_frame.data_size = 0;
    }
}

} // namespace goggles
