#include "compositor_state.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <vector>

extern "C" {
#include <wlr/render/pass.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/xcursor.h>
}

#include <goggles/profiling.hpp>
#include <util/drm_fourcc.hpp>
#include <util/logging.hpp>

namespace goggles::compositor {

namespace {

constexpr uint32_t CURSOR_SIZE = 64;
constexpr uint32_t FALLBACK_CURSOR_WIDTH = 16;
constexpr uint32_t FALLBACK_CURSOR_HEIGHT = 24;
constexpr uint32_t FALLBACK_HOTSPOT_X = 0;
constexpr uint32_t FALLBACK_HOTSPOT_Y = 0;

auto get_time_msec() -> uint32_t {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint32_t>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

auto fallback_cursor_pixels() -> std::vector<uint32_t> {
    constexpr uint32_t OPAQUE_BLACK = 0xFF000000;
    constexpr uint32_t OPAQUE_WHITE = 0xFFFFFFFF;

    std::vector<uint32_t> pixels(static_cast<size_t>(FALLBACK_CURSOR_WIDTH) *
                                     static_cast<size_t>(FALLBACK_CURSOR_HEIGHT),
                                 0U);

    auto set_pixel = [&pixels](uint32_t x, uint32_t y, uint32_t color) {
        if (x >= FALLBACK_CURSOR_WIDTH || y >= FALLBACK_CURSOR_HEIGHT) {
            return;
        }
        pixels[static_cast<size_t>(y) * FALLBACK_CURSOR_WIDTH + x] = color;
    };

    for (uint32_t y = 0; y < 12; ++y) {
        set_pixel(0, y, OPAQUE_BLACK);
        if (y == 0) {
            set_pixel(1, y, OPAQUE_BLACK);
        } else {
            set_pixel(1, y, OPAQUE_WHITE);
            set_pixel(2, y, OPAQUE_BLACK);
        }
    }

    for (uint32_t i = 0; i < 9; ++i) {
        const uint32_t x = i;
        const uint32_t y = i;
        set_pixel(x, y, OPAQUE_BLACK);
        if (x + 1 < FALLBACK_CURSOR_WIDTH) {
            set_pixel(x + 1, y, OPAQUE_WHITE);
        }
        if (y + 1 < FALLBACK_CURSOR_HEIGHT) {
            set_pixel(x, y + 1, OPAQUE_WHITE);
        }
    }

    for (uint32_t y = 10; y < FALLBACK_CURSOR_HEIGHT; ++y) {
        set_pixel(4, y, OPAQUE_BLACK);
        set_pixel(5, y, OPAQUE_WHITE);
        set_pixel(6, y, OPAQUE_BLACK);
    }

    for (uint32_t x = 0; x < 9; ++x) {
        set_pixel(x, 12, OPAQUE_BLACK);
    }

    return pixels;
}

} // namespace

void CompositorServer::set_cursor_visible(bool visible) {
    m_impl->state.set_cursor_visible(visible);
}

auto CompositorState::setup_cursor_theme() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    clear_cursor_theme();

    cursor_theme = wlr_xcursor_theme_load(nullptr, CURSOR_SIZE);
    if (!cursor_theme) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to load system cursor theme");
    }

    struct CursorImageSource {
        const uint8_t* pixels = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t hotspot_x = 0;
        uint32_t hotspot_y = 0;
        uint32_t delay_ms = 0;
    };

    auto append_frame_from_buffer = [this](const CursorImageSource& source) -> Result<void> {
        if (!source.pixels || source.width == 0 || source.height == 0) {
            return make_error<void>(ErrorCode::input_init_failed,
                                    "Cursor frame contains invalid pixel buffer");
        }

        wlr_texture* texture =
            wlr_texture_from_pixels(renderer, util::DRM_FORMAT_ARGB8888, source.width * 4,
                                    source.width, source.height, source.pixels);
        if (!texture) {
            return make_error<void>(ErrorCode::input_init_failed,
                                    "Failed to create cursor texture");
        }

        CursorFrame frame{};
        frame.texture = texture;
        frame.width = source.width;
        frame.height = source.height;
        frame.hotspot_x = std::min(source.hotspot_x, source.width - 1);
        frame.hotspot_y = std::min(source.hotspot_y, source.height - 1);
        frame.delay_ms = source.delay_ms;
        cursor_frames.push_back(frame);
        return {};
    };

    constexpr std::array<const char*, 4> CURSOR_NAMES = {
        "left_ptr",
        "default",
        "arrow",
        "pointer",
    };

    for (const char* cursor_name : CURSOR_NAMES) {
        cursor_shape = wlr_xcursor_theme_get_cursor(cursor_theme, cursor_name);
        if (cursor_shape) {
            break;
        }
    }
    cursor_frames.clear();

    if (cursor_shape) {
        cursor_frames.reserve(cursor_shape->image_count);
        for (unsigned int i = 0; i < cursor_shape->image_count; ++i) {
            const auto* image = cursor_shape->images[i];
            if (!image || !image->buffer || image->width == 0 || image->height == 0) {
                continue;
            }
            CursorImageSource source{};
            source.pixels = image->buffer;
            source.width = image->width;
            source.height = image->height;
            source.hotspot_x = image->hotspot_x;
            source.hotspot_y = image->hotspot_y;
            source.delay_ms = image->delay;
            auto frame_result = append_frame_from_buffer(source);
            if (!frame_result) {
                clear_cursor_theme();
                return frame_result;
            }
        }

        if (!cursor_frames.empty()) {
            return {};
        }

        GOGGLES_LOG_WARN("System cursor shape '{}' had no usable image frames; using built-in "
                         "fallback cursor",
                         cursor_shape->name ? cursor_shape->name : "<unnamed>");
    } else {
        GOGGLES_LOG_WARN("System cursor shape not found; using built-in fallback cursor");
    }

    const auto fallback_pixels = fallback_cursor_pixels();
    CursorImageSource fallback_source{};
    fallback_source.pixels = reinterpret_cast<const uint8_t*>(fallback_pixels.data());
    fallback_source.width = FALLBACK_CURSOR_WIDTH;
    fallback_source.height = FALLBACK_CURSOR_HEIGHT;
    fallback_source.hotspot_x = FALLBACK_HOTSPOT_X;
    fallback_source.hotspot_y = FALLBACK_HOTSPOT_Y;
    fallback_source.delay_ms = 0;
    auto fallback_result = append_frame_from_buffer(fallback_source);
    if (!fallback_result) {
        clear_cursor_theme();
        return fallback_result;
    }

    cursor_shape = nullptr;

    return {};
}

void CompositorState::clear_cursor_theme() {
    for (auto& frame : cursor_frames) {
        if (frame.texture) {
            wlr_texture_destroy(frame.texture);
            frame.texture = nullptr;
        }
    }
    cursor_frames.clear();
    cursor_shape = nullptr;
    if (cursor_theme) {
        wlr_xcursor_theme_destroy(cursor_theme);
        cursor_theme = nullptr;
    }
}

auto CompositorState::get_cursor_frame(uint32_t time_msec) const -> const CursorFrame* {
    if (cursor_frames.empty()) {
        return nullptr;
    }

    if (!cursor_shape) {
        return &cursor_frames.front();
    }

    const int frame_index = wlr_xcursor_frame(cursor_shape, time_msec);
    if (frame_index < 0 || static_cast<size_t>(frame_index) >= cursor_frames.size()) {
        return nullptr;
    }
    return &cursor_frames[static_cast<size_t>(frame_index)];
}

void CompositorState::set_cursor_visible(bool visible) {
    bool previous = cursor_visible.exchange(visible, std::memory_order_acq_rel);
    if (previous != visible) {
        request_present_reset();
    }
}

void CompositorState::render_cursor_overlay(wlr_render_pass* pass) const {
    const bool show_cursor =
        cursor_visible.load(std::memory_order_acquire) &&
        (!active_constraint || active_constraint->type != WLR_POINTER_CONSTRAINT_V1_LOCKED);
    if (!show_cursor || !cursor_initialized || present_width == 0 || present_height == 0) {
        return;
    }

    const auto* frame = get_cursor_frame(get_time_msec());
    if (!frame || !frame->texture) {
        return;
    }

    const int center_x = static_cast<int>(std::lround(cursor_x));
    const int center_y = static_cast<int>(std::lround(cursor_y));
    const int min_x = -static_cast<int>(frame->hotspot_x);
    const int min_y = -static_cast<int>(frame->hotspot_y);
    const int max_x = static_cast<int>(present_width - 1) - static_cast<int>(frame->hotspot_x);
    const int max_y = static_cast<int>(present_height - 1) - static_cast<int>(frame->hotspot_y);
    const int draw_x = std::clamp(center_x - static_cast<int>(frame->hotspot_x), min_x, max_x);
    const int draw_y = std::clamp(center_y - static_cast<int>(frame->hotspot_y), min_y, max_y);

    wlr_render_texture_options options{};
    options.texture = frame->texture;
    options.src_box = wlr_fbox{
        .x = 0.0,
        .y = 0.0,
        .width = static_cast<double>(frame->width),
        .height = static_cast<double>(frame->height),
    };
    options.dst_box = wlr_box{
        .x = draw_x,
        .y = draw_y,
        .width = static_cast<int>(frame->width),
        .height = static_cast<int>(frame->height),
    };
    options.filter_mode = WLR_SCALE_FILTER_NEAREST;
    options.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;
    wlr_render_pass_add_texture(pass, &options);
}

} // namespace goggles::compositor
