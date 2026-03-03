#pragma once

#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/// @brief Returns an error if a Vulkan call does not return `vk::Result::eSuccess`.
/// @note Usage:
///   `VK_TRY(cmd.reset(), ErrorCode::vulkan_device_lost, "Command buffer reset failed");`
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define VK_TRY(call, code, msg)                                                                    \
    do {                                                                                           \
        if (auto _vk_result = (call); _vk_result != vk::Result::eSuccess)                          \
            return nonstd::make_unexpected(                                                        \
                goggles::Error{code, std::string(msg) + ": " + vk::to_string(_vk_result)});        \
    } while (0)

// NOLINTEND(cppcoreguidelines-macro-usage)
