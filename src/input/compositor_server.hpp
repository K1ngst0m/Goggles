#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <util/error.hpp>

namespace goggles::input {

enum class InputEventType : std::uint8_t { key, pointer_motion, pointer_button, pointer_axis };

struct InputEvent {
    InputEventType type;
    uint32_t code;
    bool pressed;
    double x, y;
    double value;
    bool horizontal;
};

class CompositorServer {
public:
    CompositorServer();
    ~CompositorServer();

    CompositorServer(const CompositorServer&) = delete;
    CompositorServer& operator=(const CompositorServer&) = delete;
    CompositorServer(CompositorServer&&) = delete;
    CompositorServer& operator=(CompositorServer&&) = delete;

    [[nodiscard]] auto start() -> Result<void>;
    void stop();
    [[nodiscard]] auto x11_display() const -> std::string;
    [[nodiscard]] auto wayland_display() const -> std::string;

    [[nodiscard]] auto inject_key(uint32_t linux_keycode, bool pressed) -> bool;
    [[nodiscard]] auto inject_pointer_motion(double sx, double sy) -> bool;
    [[nodiscard]] auto inject_pointer_button(uint32_t button, bool pressed) -> bool;
    [[nodiscard]] auto inject_pointer_axis(double value, bool horizontal) -> bool;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace goggles::input
