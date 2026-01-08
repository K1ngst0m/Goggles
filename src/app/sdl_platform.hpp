#pragma once

#include <string>
#include <util/error.hpp>

namespace goggles::app {

struct WindowHandle {
    void* ptr = nullptr;
};

struct EventRef {
    const void* ptr = nullptr;
};

class SdlPlatform {
public:
    struct CreateInfo {
        std::string title;
        int width = 0;
        int height = 0;
        bool enable_vulkan = true;
        bool resizable = true;
    };

    [[nodiscard]] static auto create(const CreateInfo& create_info) -> ResultPtr<SdlPlatform>;

    ~SdlPlatform();

    SdlPlatform(const SdlPlatform&) = delete;
    SdlPlatform& operator=(const SdlPlatform&) = delete;
    SdlPlatform(SdlPlatform&&) = delete;
    SdlPlatform& operator=(SdlPlatform&&) = delete;

    void shutdown();

    [[nodiscard]] auto window() const -> WindowHandle { return {.ptr = m_window}; }

private:
    SdlPlatform() = default;

    void* m_window = nullptr;
    bool m_sdl_initialized = false;
};

} // namespace goggles::app
