// Capture receiver - socket server for receiving frames from layer

#pragma once

#include <capture/capture_protocol.hpp>

#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>

namespace goggles {

// =============================================================================
// Capture Frame
// =============================================================================

struct CaptureFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    void* data = nullptr;        // mmap'd DMA-BUF data
    size_t data_size = 0;
    int dmabuf_fd = -1;
};

// =============================================================================
// Capture Receiver
// =============================================================================

class CaptureReceiver {
public:
    CaptureReceiver();
    ~CaptureReceiver();

    // Non-copyable
    CaptureReceiver(const CaptureReceiver&) = delete;
    CaptureReceiver& operator=(const CaptureReceiver&) = delete;

    // Initialize socket server
    bool init();
    void shutdown();

    // Poll for new frames (non-blocking)
    // Returns true if a new frame is available
    bool poll_frame();

    // Get current frame (valid after poll_frame returns true)
    const CaptureFrame& get_frame() const { return frame_; }

    // Create or update SDL texture from current frame
    SDL_Texture* update_texture(SDL_Renderer* renderer, SDL_Texture* existing);

    // State
    bool is_connected() const { return client_fd_ >= 0; }
    bool has_frame() const { return frame_.data != nullptr; }

private:
    bool accept_client();
    bool receive_message();
    void cleanup_frame();
    void unmap_frame();

    int listen_fd_ = -1;     // Listening socket
    int client_fd_ = -1;     // Connected client (layer)
    CaptureFrame frame_{};
    capture::CaptureTextureData last_texture_{};
};

} // namespace goggles
