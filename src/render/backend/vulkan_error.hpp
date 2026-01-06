#pragma once

#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/// Early-return if vk::Result is not eSuccess, creating an error with the given code and message.
/// Uses make_unexpected so return type is inferred from the calling function.
/// Usage: VK_TRY(cmd.reset(), ErrorCode::vulkan_device_lost, "Command buffer reset failed");
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define VK_TRY(call, code, msg)                                                                    \
    do {                                                                                           \
        if (auto _vk_result = (call); _vk_result != vk::Result::eSuccess)                          \
            return nonstd::make_unexpected(                                                        \
                goggles::Error{code, std::string(msg) + ": " + vk::to_string(_vk_result)});        \
    } while (0)

// NOLINTEND(cppcoreguidelines-macro-usage)
