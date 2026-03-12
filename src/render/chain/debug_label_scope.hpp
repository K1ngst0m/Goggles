#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

using BeginDebugLabelFn = void (*)(vk::CommandBuffer, const vk::DebugUtilsLabelEXT&);
using EndDebugLabelFn = void (*)(vk::CommandBuffer);

inline void begin_debug_label(vk::CommandBuffer cmd, const vk::DebugUtilsLabelEXT& label) {
    cmd.beginDebugUtilsLabelEXT(label);
}

inline void end_debug_label(vk::CommandBuffer cmd) {
    cmd.endDebugUtilsLabelEXT();
}

struct DebugLabelDispatch {
    BeginDebugLabelFn begin = &begin_debug_label;
    EndDebugLabelFn end = &end_debug_label;
    bool enabled = VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBeginDebugUtilsLabelEXT != nullptr &&
                   VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdEndDebugUtilsLabelEXT != nullptr;

    [[nodiscard]] auto is_available() const -> bool {
        return enabled && begin != nullptr && end != nullptr;
    }
};

class ScopedDebugLabel {
public:
    ScopedDebugLabel(vk::CommandBuffer cmd, std::string_view name,
                     const std::array<float, 4>& color, DebugLabelDispatch dispatch = {})
        : m_command_buffer(cmd) {
        if (!dispatch.is_available()) {
            return;
        }

        const std::string label_name{name};
        vk::DebugUtilsLabelEXT label{};
        label.pLabelName = label_name.c_str();
        std::copy(color.begin(), color.end(), label.color.begin());
        dispatch.begin(cmd, label);
        m_end = dispatch.end;
    }

    ~ScopedDebugLabel() {
        if (m_end != nullptr) {
            m_end(m_command_buffer);
        }
    }

    ScopedDebugLabel(const ScopedDebugLabel&) = delete;
    auto operator=(const ScopedDebugLabel&) -> ScopedDebugLabel& = delete;
    ScopedDebugLabel(ScopedDebugLabel&&) = delete;
    auto operator=(ScopedDebugLabel&&) -> ScopedDebugLabel& = delete;

    [[nodiscard]] auto active() const -> bool { return m_end != nullptr; }

private:
    vk::CommandBuffer m_command_buffer;
    EndDebugLabelFn m_end = nullptr;
};

} // namespace goggles::render
