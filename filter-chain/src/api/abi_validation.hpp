#pragma once

#include "runtime/chain.hpp"
#include "runtime/device.hpp"
#include "runtime/instance.hpp"
#include "runtime/program.hpp"

#include <cstdint>
#include <goggles_filter_chain.h>

namespace goggles::filter_chain::detail {

// ── Struct-size validation ──────────────────────────────────────────────────

/// @brief Validate that a struct_size field matches the expected compile-time size.
[[nodiscard]] inline auto validate_struct_size(uint32_t actual, uint32_t expected) -> bool {
    return actual == expected;
}

/// @brief Validate a non-null pointer to a creation info struct with struct_size check.
template <typename T>
[[nodiscard]] inline auto validate_create_info(const T* info) -> goggles_fc_status_t {
    if (info == nullptr) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    if (!validate_struct_size(info->struct_size, GOGGLES_FC_STRUCT_SIZE(T))) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return GOGGLES_FC_STATUS_OK;
}

// ── Handle validation (null + magic) ────────────────────────────────────────

/// @brief Validate a borrowed handle is non-null.
template <typename T>
[[nodiscard]] inline auto validate_handle(const T* handle) -> goggles_fc_status_t {
    return handle != nullptr ? GOGGLES_FC_STATUS_OK : GOGGLES_FC_STATUS_INVALID_ARGUMENT;
}

/// @brief Validate an instance handle: null check + magic number verification.
[[nodiscard]] inline auto validate_instance_handle(const goggles_fc_instance_t* handle)
    -> goggles_fc_status_t {
    if (!runtime::Instance::check_magic(handle)) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return GOGGLES_FC_STATUS_OK;
}

/// @brief Validate a device handle: null check + magic number verification.
[[nodiscard]] inline auto validate_device_handle(const goggles_fc_device_t* handle)
    -> goggles_fc_status_t {
    if (!runtime::Device::check_magic(handle)) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return GOGGLES_FC_STATUS_OK;
}

/// @brief Validate a program handle: null check + magic number verification.
[[nodiscard]] inline auto validate_program_handle(const goggles_fc_program_t* handle)
    -> goggles_fc_status_t {
    if (!runtime::Program::check_magic(handle)) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return GOGGLES_FC_STATUS_OK;
}

/// @brief Validate a chain handle: null check + magic number verification.
[[nodiscard]] inline auto validate_chain_handle(const goggles_fc_chain_t* handle)
    -> goggles_fc_status_t {
    if (!runtime::Chain::check_magic(handle)) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return GOGGLES_FC_STATUS_OK;
}

// ── Output pointer validation ───────────────────────────────────────────────

/// @brief Validate an output pointer is non-null.
template <typename T>
[[nodiscard]] inline auto validate_output(T** out) -> goggles_fc_status_t {
    return out != nullptr ? GOGGLES_FC_STATUS_OK : GOGGLES_FC_STATUS_INVALID_ARGUMENT;
}

// ── UTF-8 validation ────────────────────────────────────────────────────────

/// @brief Perform byte-level UTF-8 sequence validation.
///
/// Checks continuation byte patterns, multibyte sequence lengths, rejects
/// overlong encodings, surrogate codepoints (U+D800-U+DFFF), and codepoints
/// above U+10FFFF.
[[nodiscard]] inline auto is_valid_utf8(const char* data, size_t size) -> bool {
    const auto* bytes = reinterpret_cast<const uint8_t*>(data);
    size_t i = 0;

    while (i < size) {
        uint8_t b0 = bytes[i];
        uint32_t codepoint = 0;
        size_t seq_len = 0;

        if (b0 <= 0x7F) {
            // 1-byte sequence: 0xxxxxxx
            ++i;
            continue;
        }

        if ((b0 & 0xE0) == 0xC0) {
            // 2-byte sequence: 110xxxxx 10xxxxxx
            seq_len = 2;
            codepoint = b0 & 0x1Fu;
            // Reject overlong: codepoint must be >= 0x80 (needs at least 2 bytes)
            // Minimum valid first byte for non-overlong: 0xC2
            if (b0 < 0xC2) {
                return false;
            }
        } else if ((b0 & 0xF0) == 0xE0) {
            // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
            seq_len = 3;
            codepoint = b0 & 0x0Fu;
        } else if ((b0 & 0xF8) == 0xF0) {
            // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            seq_len = 4;
            codepoint = b0 & 0x07u;
            // Reject first bytes that would produce codepoints > U+10FFFF
            if (b0 > 0xF4) {
                return false;
            }
        } else {
            // Invalid leading byte (continuation byte in leading position, or 0xFE/0xFF)
            return false;
        }

        // Check that we have enough bytes remaining
        if (i + seq_len > size) {
            return false;
        }

        // Validate continuation bytes and accumulate codepoint
        for (size_t j = 1; j < seq_len; ++j) {
            uint8_t bj = bytes[i + j];
            if ((bj & 0xC0) != 0x80) {
                return false; // Not a valid continuation byte (10xxxxxx)
            }
            codepoint = (codepoint << 6) | (bj & 0x3Fu);
        }

        // Reject overlong encodings
        if (seq_len == 3 && codepoint < 0x0800u) {
            return false;
        }
        if (seq_len == 4 && codepoint < 0x10000u) {
            return false;
        }

        // Reject surrogate codepoints (U+D800 - U+DFFF)
        if (codepoint >= 0xD800u && codepoint <= 0xDFFFu) {
            return false;
        }

        // Reject codepoints above U+10FFFF
        if (codepoint > 0x10FFFFu) {
            return false;
        }

        i += seq_len;
    }

    return true;
}

/// @brief Validate a UTF-8 view: if data is non-null, the content must be valid UTF-8.
///        A null data pointer with non-zero size is invalid.
///        A null data pointer with zero size is valid (absent optional field).
[[nodiscard]] inline auto validate_utf8_view_optional(const goggles_fc_utf8_view_t& view) -> bool {
    if (view.data == nullptr) {
        return view.size == 0;
    }
    if (view.size == 0) {
        return true;
    }
    return is_valid_utf8(view.data, view.size);
}

/// @brief Validate a UTF-8 view that is required (must be non-null, non-empty, and valid UTF-8).
[[nodiscard]] inline auto validate_utf8_view_required(const goggles_fc_utf8_view_t& view) -> bool {
    if (view.data == nullptr || view.size == 0) {
        return false;
    }
    return is_valid_utf8(view.data, view.size);
}

// ── Domain-specific validation ──────────────────────────────────────────────

/// @brief Validate VkDevice-create info fields specific to the Vulkan device binding.
[[nodiscard]] inline auto
validate_vk_device_create_info(const goggles_fc_vk_device_create_info_t* info)
    -> goggles_fc_status_t {
    auto status = validate_create_info(info);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    if (info->physical_device == VK_NULL_HANDLE || info->device == VK_NULL_HANDLE ||
        info->graphics_queue == VK_NULL_HANDLE) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    if (!validate_utf8_view_optional(info->cache_dir)) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return GOGGLES_FC_STATUS_OK;
}

/// @brief Validate a preset source descriptor.
[[nodiscard]] inline auto validate_preset_source(const goggles_fc_preset_source_t* source)
    -> goggles_fc_status_t {
    auto status = validate_create_info(source);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    // Validate UTF-8 on public text fields shared by all source kinds.
    if (!validate_utf8_view_optional(source->source_name)) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    if (!validate_utf8_view_optional(source->base_path)) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }

    if (source->kind == GOGGLES_FC_PRESET_SOURCE_FILE) {
        // File sources accept two valid forms:
        //   1. path.data != nullptr && path.size > 0  → filesystem preset path (must be valid
        //   UTF-8)
        //   2. path.data != nullptr && path.size == 0  → passthrough sentinel: no preset is loaded;
        //      SourceResolver returns empty bytes, and ChainBuilder creates a single-pass blit
        //      pipeline. Used by load_passthrough_into_slot() and tested in contract tests.
        // path.data == nullptr is always rejected regardless of size.
        if (source->path.data == nullptr) {
            return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
        }
        if (source->path.size > 0 && !is_valid_utf8(source->path.data, source->path.size)) {
            return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
        }
    } else if (source->kind == GOGGLES_FC_PRESET_SOURCE_MEMORY) {
        if (source->bytes == nullptr || source->byte_count == 0) {
            return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
        }
        // For memory sources, path is optional but must be valid UTF-8 if present.
        if (!validate_utf8_view_optional(source->path)) {
            return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
        }
    } else {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return GOGGLES_FC_STATUS_OK;
}

/// @brief Validate chain-create info fields.
[[nodiscard]] inline auto validate_chain_create_info(const goggles_fc_chain_create_info_t* info)
    -> goggles_fc_status_t {
    auto status = validate_create_info(info);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    if (info->target_format == VK_FORMAT_UNDEFINED) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    if (info->frames_in_flight == 0) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return GOGGLES_FC_STATUS_OK;
}

/// @brief Validate record-info fields.
[[nodiscard]] inline auto validate_record_info(const goggles_fc_record_info_vk_t* info)
    -> goggles_fc_status_t {
    auto status = validate_create_info(info);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    if (info->command_buffer == VK_NULL_HANDLE || info->source_image == VK_NULL_HANDLE ||
        info->source_view == VK_NULL_HANDLE || info->target_view == VK_NULL_HANDLE) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    if (info->source_extent.width == 0 || info->source_extent.height == 0 ||
        info->target_extent.width == 0 || info->target_extent.height == 0) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return GOGGLES_FC_STATUS_OK;
}

/// @brief Validate that a frame index is within bounds.
///
/// This is a separate function from validate_record_info because the record-info
/// struct does not carry frames_in_flight — that value lives on the Chain object.
/// Call this from the recording call site (e.g. chain.cpp record_vk or c_api.cpp)
/// where both the frame_index and the chain's frames_in_flight are available.
[[nodiscard]] inline auto validate_frame_index(uint32_t frame_index, uint32_t frames_in_flight)
    -> goggles_fc_status_t {
    if (frame_index >= frames_in_flight) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return GOGGLES_FC_STATUS_OK;
}

/// @brief Validate a control semantic identity (stage + required name).
[[nodiscard]] inline auto validate_control_identity(uint32_t stage, goggles_fc_utf8_view_t name)
    -> goggles_fc_status_t {
    if (stage != GOGGLES_FC_STAGE_PRECHAIN && stage != GOGGLES_FC_STAGE_EFFECT) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    if (!validate_utf8_view_required(name)) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return GOGGLES_FC_STATUS_OK;
}

} // namespace goggles::filter_chain::detail
