#pragma once

namespace goggles::capture {

/// @brief Compositor/framebuffer capture backend.
class CompositorCapture {
public:
    CompositorCapture() = default;
    ~CompositorCapture() = default;

    CompositorCapture(const CompositorCapture&) = delete;
    CompositorCapture& operator=(const CompositorCapture&) = delete;
    CompositorCapture(CompositorCapture&&) = delete;
    CompositorCapture& operator=(CompositorCapture&&) = delete;
};

} // namespace goggles::capture
