#pragma once

#include <algorithm>
#include <array>
#include <string_view>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

struct DebugLabelDispatch {
    PFN_vkCmdBeginDebugUtilsLabelEXT begin =
        VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBeginDebugUtilsLabelEXT;
    PFN_vkCmdEndDebugUtilsLabelEXT end = VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdEndDebugUtilsLabelEXT;

    [[nodiscard]] auto is_available() const -> bool { return begin != nullptr && end != nullptr; }
};

class ScopedDebugLabel {
public:
    ScopedDebugLabel(vk::CommandBuffer cmd, std::string_view name,
                     const std::array<float, 4>& color, DebugLabelDispatch dispatch = {})
        : m_command_buffer(cmd) {
        if (!dispatch.is_available()) {
            return;
        }

        VkDebugUtilsLabelEXT label{};
        label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name.data();
        std::copy(color.begin(), color.end(), label.color);
        dispatch.begin(static_cast<VkCommandBuffer>(cmd), &label);
        m_end = dispatch.end;
    }

    ~ScopedDebugLabel() {
        if (m_end != nullptr) {
            m_end(static_cast<VkCommandBuffer>(m_command_buffer));
        }
    }

    ScopedDebugLabel(const ScopedDebugLabel&) = delete;
    auto operator=(const ScopedDebugLabel&) -> ScopedDebugLabel& = delete;
    ScopedDebugLabel(ScopedDebugLabel&&) = delete;
    auto operator=(ScopedDebugLabel&&) -> ScopedDebugLabel& = delete;

    [[nodiscard]] auto active() const -> bool { return m_end != nullptr; }

private:
    vk::CommandBuffer m_command_buffer;
    PFN_vkCmdEndDebugUtilsLabelEXT m_end = nullptr;
};

} // namespace goggles::render
