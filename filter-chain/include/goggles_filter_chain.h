#ifndef GOGGLES_FILTER_CHAIN_H
#define GOGGLES_FILTER_CHAIN_H

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

#include <vulkan/vulkan.h>

/* ── Visibility / calling-convention ─────────────────────────────────────── */

#if defined(_WIN32)
#if defined(GOGGLES_FC_BUILD_SHARED)
#define GOGGLES_FC_API __declspec(dllexport)
#elif defined(GOGGLES_FC_USE_SHARED)
#define GOGGLES_FC_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && defined(GOGGLES_FC_BUILD_SHARED)
#define GOGGLES_FC_API __attribute__((visibility("default")))
#endif

#ifndef GOGGLES_FC_API
#define GOGGLES_FC_API
#endif

#ifndef GOGGLES_FC_CALL
#if defined(_WIN32)
#define GOGGLES_FC_CALL __cdecl
#else
#define GOGGLES_FC_CALL
#endif
#endif

/* ── Version macros ──────────────────────────────────────────────────────── */

#define GOGGLES_FC_MAKE_VERSION(major, minor, patch)                                               \
    ((((uint32_t)(major) & 0x3ffu) << 22) | (((uint32_t)(minor) & 0x3ffu) << 12) |                 \
     ((uint32_t)(patch) & 0xfffu))

#define GOGGLES_FC_API_VERSION GOGGLES_FC_MAKE_VERSION(1u, 0u, 0u)
#define GOGGLES_FC_ABI_VERSION 1u
#define GOGGLES_FC_STRUCT_SIZE(type) ((uint32_t)sizeof(type))

/* ── Status codes ────────────────────────────────────────────────────────── */

#define GOGGLES_FC_STATUS_OK ((uint32_t)0u)
#define GOGGLES_FC_STATUS_INVALID_ARGUMENT ((uint32_t)1u)
#define GOGGLES_FC_STATUS_NOT_INITIALIZED ((uint32_t)2u)
#define GOGGLES_FC_STATUS_NOT_FOUND ((uint32_t)3u)
#define GOGGLES_FC_STATUS_PRESET_ERROR ((uint32_t)4u)
#define GOGGLES_FC_STATUS_IO_ERROR ((uint32_t)5u)
#define GOGGLES_FC_STATUS_VULKAN_ERROR ((uint32_t)6u)
#define GOGGLES_FC_STATUS_OUT_OF_MEMORY ((uint32_t)7u)
#define GOGGLES_FC_STATUS_NOT_SUPPORTED ((uint32_t)8u)
#define GOGGLES_FC_STATUS_RUNTIME_ERROR ((uint32_t)9u)
#define GOGGLES_FC_STATUS_INVALID_DEPENDENCY ((uint32_t)10u)
#define GOGGLES_FC_STATUS_VALIDATION_ERROR ((uint32_t)11u)

/* ── Capability flags ────────────────────────────────────────────────────── */

#define GOGGLES_FC_CAPABILITY_NONE ((uint32_t)0u)
#define GOGGLES_FC_CAPABILITY_VULKAN ((uint32_t)(1u << 0))
#define GOGGLES_FC_CAPABILITY_FILE_SOURCE ((uint32_t)(1u << 1))
#define GOGGLES_FC_CAPABILITY_MEMORY_SOURCE ((uint32_t)(1u << 2))
#define GOGGLES_FC_CAPABILITY_LOG_CALLBACK ((uint32_t)(1u << 3))

/* ── Log levels ──────────────────────────────────────────────────────────── */

#define GOGGLES_FC_LOG_LEVEL_TRACE ((uint32_t)0u)
#define GOGGLES_FC_LOG_LEVEL_DEBUG ((uint32_t)1u)
#define GOGGLES_FC_LOG_LEVEL_INFO ((uint32_t)2u)
#define GOGGLES_FC_LOG_LEVEL_WARN ((uint32_t)3u)
#define GOGGLES_FC_LOG_LEVEL_ERROR ((uint32_t)4u)
#define GOGGLES_FC_LOG_LEVEL_CRITICAL ((uint32_t)5u)

/* ── Preset source kinds ─────────────────────────────────────────────────── */

#define GOGGLES_FC_PRESET_SOURCE_FILE ((uint32_t)0u)
#define GOGGLES_FC_PRESET_SOURCE_MEMORY ((uint32_t)1u)

/* ── Scale modes ─────────────────────────────────────────────────────────── */

#define GOGGLES_FC_SCALE_MODE_STRETCH ((uint32_t)0u)
#define GOGGLES_FC_SCALE_MODE_FIT ((uint32_t)1u)
#define GOGGLES_FC_SCALE_MODE_INTEGER ((uint32_t)2u)
#define GOGGLES_FC_SCALE_MODE_FILL ((uint32_t)3u)
#define GOGGLES_FC_SCALE_MODE_DYNAMIC ((uint32_t)4u)

/* ── Stage identifiers and masks ─────────────────────────────────────────── */

#define GOGGLES_FC_STAGE_PRECHAIN ((uint32_t)0u)
#define GOGGLES_FC_STAGE_EFFECT ((uint32_t)1u)
#define GOGGLES_FC_STAGE_POSTCHAIN ((uint32_t)2u)

#define GOGGLES_FC_STAGE_MASK_PRECHAIN ((uint32_t)(1u << 0))
#define GOGGLES_FC_STAGE_MASK_EFFECT ((uint32_t)(1u << 1))
#define GOGGLES_FC_STAGE_MASK_POSTCHAIN ((uint32_t)(1u << 2))
#define GOGGLES_FC_STAGE_MASK_ALL                                                                  \
    ((uint32_t)(GOGGLES_FC_STAGE_MASK_PRECHAIN | GOGGLES_FC_STAGE_MASK_EFFECT |                    \
                GOGGLES_FC_STAGE_MASK_POSTCHAIN))

/* ── Provenance kinds ────────────────────────────────────────────────────── */

#define GOGGLES_FC_PROVENANCE_FILE ((uint32_t)0u)
#define GOGGLES_FC_PROVENANCE_MEMORY ((uint32_t)1u)

/* ── Diagnostic capture modes ────────────────────────────────────────────── */

#define GOGGLES_FC_DIAGNOSTIC_CAPTURE_MINIMAL ((uint32_t)0u)
#define GOGGLES_FC_DIAGNOSTIC_CAPTURE_STANDARD ((uint32_t)1u)
#define GOGGLES_FC_DIAGNOSTIC_CAPTURE_INVESTIGATE ((uint32_t)2u)
#define GOGGLES_FC_DIAGNOSTIC_CAPTURE_FORENSIC ((uint32_t)3u)

/* ── Diagnostic activation tiers ─────────────────────────────────────────── */

#define GOGGLES_FC_DIAGNOSTIC_TIER_0 ((uint32_t)0u)
#define GOGGLES_FC_DIAGNOSTIC_TIER_1 ((uint32_t)1u)
#define GOGGLES_FC_DIAGNOSTIC_TIER_2 ((uint32_t)2u)

#ifdef __cplusplus
extern "C" {
#endif

/* ── Portable null / void-arg helpers ────────────────────────────────────── */

#ifdef __cplusplus
#define GOGGLES_FC_NOARGS
#define GOGGLES_FC_NULLPTR nullptr
#else
#define GOGGLES_FC_NOARGS void
#define GOGGLES_FC_NULLPTR NULL
#endif

/* ── Scalar typedefs ─────────────────────────────────────────────────────── */

#ifdef __cplusplus
using goggles_fc_status_t = uint32_t;
using goggles_fc_log_level_t = uint32_t;
using goggles_fc_capability_flags_t = uint32_t;
using goggles_fc_preset_source_kind_t = uint32_t;
#else
typedef uint32_t goggles_fc_status_t;
typedef uint32_t goggles_fc_log_level_t;
typedef uint32_t goggles_fc_capability_flags_t;
typedef uint32_t goggles_fc_preset_source_kind_t;
#endif

/* ── Opaque handle forward declarations ──────────────────────────────────── */

struct goggles_fc_instance;
struct goggles_fc_device;
struct goggles_fc_program;
struct goggles_fc_chain;

#ifdef __cplusplus
using goggles_fc_instance_t = goggles_fc_instance;
using goggles_fc_device_t = goggles_fc_device;
using goggles_fc_program_t = goggles_fc_program;
using goggles_fc_chain_t = goggles_fc_chain;
#else
typedef struct goggles_fc_instance goggles_fc_instance_t;
typedef struct goggles_fc_device goggles_fc_device_t;
typedef struct goggles_fc_program goggles_fc_program_t;
typedef struct goggles_fc_chain goggles_fc_chain_t;
#endif

/* ── POD structs: UTF-8 view ─────────────────────────────────────────────── */

struct GogglesFcUtf8View {
    const char* data;
    size_t size;
};

struct GogglesFcExtent2D {
    uint32_t width;
    uint32_t height;
};

#ifdef __cplusplus
using goggles_fc_utf8_view_t = GogglesFcUtf8View;
using goggles_fc_extent_2d_t = GogglesFcExtent2D;
#else
typedef struct GogglesFcUtf8View goggles_fc_utf8_view_t;
typedef struct GogglesFcExtent2D goggles_fc_extent_2d_t;
#endif

/* ── POD structs: logging callback ───────────────────────────────────────── */

struct GogglesFcLogMessage {
    uint32_t struct_size;
    goggles_fc_log_level_t level;
    goggles_fc_utf8_view_t domain;
    goggles_fc_utf8_view_t message;
};

#ifdef __cplusplus
using goggles_fc_log_message_t = GogglesFcLogMessage;
#else
typedef struct GogglesFcLogMessage goggles_fc_log_message_t;
#endif

#ifdef __cplusplus
using goggles_fc_log_callback_t = void(GOGGLES_FC_CALL*)(const goggles_fc_log_message_t* message,
                                                         void* user_data);
#else
typedef void(GOGGLES_FC_CALL* goggles_fc_log_callback_t)(const goggles_fc_log_message_t* message,
                                                         void* user_data);
#endif

/* ── POD structs: instance creation ──────────────────────────────────────── */

struct GogglesFcInstanceCreateInfo {
    uint32_t struct_size;
    goggles_fc_log_callback_t log_callback;
    void* log_user_data;
};

#ifdef __cplusplus
using goggles_fc_instance_create_info_t = GogglesFcInstanceCreateInfo;
#else
typedef struct GogglesFcInstanceCreateInfo goggles_fc_instance_create_info_t;
#endif

/* ── POD structs: Vulkan device creation ─────────────────────────────────── */

struct GogglesFcVkDeviceCreateInfo {
    uint32_t struct_size;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family_index;
    goggles_fc_utf8_view_t cache_dir;
};

#ifdef __cplusplus
using goggles_fc_vk_device_create_info_t = GogglesFcVkDeviceCreateInfo;
#else
typedef struct GogglesFcVkDeviceCreateInfo goggles_fc_vk_device_create_info_t;
#endif

/* ── POD structs: import callbacks for memory-backed presets ─────────────── */

struct GogglesFcImportCallbacks;

#ifdef __cplusplus
using goggles_fc_import_result_t = goggles_fc_status_t;
using goggles_fc_import_read_fn_t = goggles_fc_import_result_t(GOGGLES_FC_CALL*)(
    goggles_fc_utf8_view_t relative_path, void** out_bytes, size_t* out_byte_count,
    void* user_data);
using goggles_fc_import_free_fn_t = void(GOGGLES_FC_CALL*)(void* bytes, size_t byte_count,
                                                           void* user_data);
#else
typedef goggles_fc_status_t goggles_fc_import_result_t;
typedef goggles_fc_import_result_t(GOGGLES_FC_CALL* goggles_fc_import_read_fn_t)(
    goggles_fc_utf8_view_t relative_path, void** out_bytes, size_t* out_byte_count,
    void* user_data);
typedef void(GOGGLES_FC_CALL* goggles_fc_import_free_fn_t)(void* bytes, size_t byte_count,
                                                           void* user_data);
#endif

struct GogglesFcImportCallbacks {
    uint32_t struct_size;
    goggles_fc_import_read_fn_t read_fn;
    goggles_fc_import_free_fn_t free_fn;
    void* user_data;
};

#ifdef __cplusplus
using goggles_fc_import_callbacks_t = GogglesFcImportCallbacks;
#else
typedef struct GogglesFcImportCallbacks goggles_fc_import_callbacks_t;
#endif

/* ── POD structs: preset source descriptor ───────────────────────────────── */

/**
 * @brief Preset source descriptor for loading filter presets.
 *
 * When @c kind is @c GOGGLES_FC_PRESET_SOURCE_FILE:
 *   - @c path.data non-null with @c path.size > 0: loads the preset from the
 *     filesystem path given by @c path.
 *   - @c path.data non-null with @c path.size == 0: passthrough mode. No preset
 *     file is loaded; the runtime creates a built-in single-pass blit pipeline
 *     that copies the source image to the target without applying any filters.
 *   - @c path.data null: rejected with @c GOGGLES_FC_STATUS_INVALID_ARGUMENT.
 */
struct GogglesFcPresetSource {
    uint32_t struct_size;
    goggles_fc_preset_source_kind_t kind;
    goggles_fc_utf8_view_t source_name;
    const void* bytes;
    size_t byte_count;
    goggles_fc_utf8_view_t path;
    goggles_fc_utf8_view_t base_path;
    const goggles_fc_import_callbacks_t* import_callbacks;
};

#ifdef __cplusplus
using goggles_fc_preset_source_t = GogglesFcPresetSource;
#else
typedef struct GogglesFcPresetSource goggles_fc_preset_source_t;
#endif

/* ── POD structs: chain creation ─────────────────────────────────────────── */

struct GogglesFcChainCreateInfo {
    uint32_t struct_size;
    VkFormat target_format;
    uint32_t frames_in_flight;
    uint32_t initial_stage_mask;
    goggles_fc_extent_2d_t initial_prechain_resolution;
};

#ifdef __cplusplus
using goggles_fc_chain_create_info_t = GogglesFcChainCreateInfo;
#else
typedef struct GogglesFcChainCreateInfo goggles_fc_chain_create_info_t;
#endif

/* ── POD structs: chain target info (retarget) ───────────────────────────── */

struct GogglesFcChainTargetInfo {
    uint32_t struct_size;
    VkFormat target_format;
};

#ifdef __cplusplus
using goggles_fc_chain_target_info_t = GogglesFcChainTargetInfo;
#else
typedef struct GogglesFcChainTargetInfo goggles_fc_chain_target_info_t;
#endif

/* ── POD structs: Vulkan record info ─────────────────────────────────────── */

struct GogglesFcRecordInfoVk {
    uint32_t struct_size;
    VkCommandBuffer command_buffer;
    VkImage source_image;
    VkImageView source_view;
    goggles_fc_extent_2d_t source_extent;
    VkImageView target_view;
    goggles_fc_extent_2d_t target_extent;
    uint32_t frame_index;
    uint32_t scale_mode;
    uint32_t integer_scale;
};

#ifdef __cplusplus
using goggles_fc_record_info_vk_t = GogglesFcRecordInfoVk;
#else
typedef struct GogglesFcRecordInfoVk goggles_fc_record_info_vk_t;
#endif

/* ── POD structs: control metadata ───────────────────────────────────────── */

struct GogglesFcControlInfo {
    uint32_t struct_size;
    uint32_t index;
    uint32_t stage;
    goggles_fc_utf8_view_t name;
    goggles_fc_utf8_view_t description;
    float current_value;
    float default_value;
    float min_value;
    float max_value;
    float step;
};

#ifdef __cplusplus
using goggles_fc_control_info_t = GogglesFcControlInfo;
#else
typedef struct GogglesFcControlInfo goggles_fc_control_info_t;
#endif

/* ── POD structs: program source info ────────────────────────────────────── */

struct GogglesFcProgramSourceInfo {
    uint32_t struct_size;
    uint32_t provenance;
    goggles_fc_utf8_view_t source_name;
    goggles_fc_utf8_view_t source_path;
    uint32_t pass_count;
};

#ifdef __cplusplus
using goggles_fc_program_source_info_t = GogglesFcProgramSourceInfo;
#else
typedef struct GogglesFcProgramSourceInfo goggles_fc_program_source_info_t;
#endif

/* ── POD structs: program report ─────────────────────────────────────────── */

struct GogglesFcProgramReport {
    uint32_t struct_size;
    uint32_t shader_count;
    uint32_t pass_count;
    uint32_t texture_count;
};

#ifdef __cplusplus
using goggles_fc_program_report_t = GogglesFcProgramReport;
#else
typedef struct GogglesFcProgramReport goggles_fc_program_report_t;
#endif

/* ── POD structs: chain report ───────────────────────────────────────────── */

struct GogglesFcChainReport {
    uint32_t struct_size;
    uint32_t pass_count;
    uint32_t frames_rendered;
    uint32_t current_stage_mask;
};

#ifdef __cplusplus
using goggles_fc_chain_report_t = GogglesFcChainReport;
#else
typedef struct GogglesFcChainReport goggles_fc_chain_report_t;
#endif

/* ── POD structs: chain error info ───────────────────────────────────────── */

struct GogglesFcChainErrorInfo {
    uint32_t struct_size;
    uint32_t status;
    int32_t vk_result;
    uint32_t subsystem_code;
};

#ifdef __cplusplus
using goggles_fc_chain_error_info_t = GogglesFcChainErrorInfo;
#else
typedef struct GogglesFcChainErrorInfo goggles_fc_chain_error_info_t;
#endif

/* ── POD structs: diagnostic policy ──────────────────────────────────────── */

struct GogglesFcDiagnosticPolicy {
    uint32_t struct_size;
    uint32_t capture_mode;
    uint32_t activation_tier;
    uint32_t capture_frame_limit;
};

#ifdef __cplusplus
using goggles_fc_diagnostic_policy_t = GogglesFcDiagnosticPolicy;
#else
typedef struct GogglesFcDiagnosticPolicy goggles_fc_diagnostic_policy_t;
#endif

/* ── POD structs: diagnostic summary ─────────────────────────────────────── */

struct GogglesFcDiagnosticSummary {
    uint32_t struct_size;
    bool session_active;
    uint32_t debug_count;
    uint32_t info_count;
    uint32_t warning_count;
    uint32_t error_count;
    uint32_t current_frame;
    uint32_t total_events;
};

#ifdef __cplusplus
using goggles_fc_diagnostic_summary_t = GogglesFcDiagnosticSummary;
#else
typedef struct GogglesFcDiagnosticSummary goggles_fc_diagnostic_summary_t;
#endif

/* ── Inline struct initializers ──────────────────────────────────────────── */

static inline goggles_fc_instance_create_info_t
goggles_fc_instance_create_info_init(GOGGLES_FC_NOARGS) {
    goggles_fc_instance_create_info_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_instance_create_info_t);
    value.log_callback = GOGGLES_FC_NULLPTR;
    value.log_user_data = GOGGLES_FC_NULLPTR;
    return value;
}

static inline goggles_fc_vk_device_create_info_t
goggles_fc_vk_device_create_info_init(GOGGLES_FC_NOARGS) {
    goggles_fc_vk_device_create_info_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_vk_device_create_info_t);
    value.physical_device = VK_NULL_HANDLE;
    value.device = VK_NULL_HANDLE;
    value.graphics_queue = VK_NULL_HANDLE;
    value.graphics_queue_family_index = 0u;
    value.cache_dir.data = GOGGLES_FC_NULLPTR;
    value.cache_dir.size = 0u;
    return value;
}

static inline goggles_fc_preset_source_t goggles_fc_preset_source_init(GOGGLES_FC_NOARGS) {
    goggles_fc_preset_source_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_preset_source_t);
    value.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    value.source_name.data = GOGGLES_FC_NULLPTR;
    value.source_name.size = 0u;
    value.bytes = GOGGLES_FC_NULLPTR;
    value.byte_count = 0u;
    value.path.data = GOGGLES_FC_NULLPTR;
    value.path.size = 0u;
    value.base_path.data = GOGGLES_FC_NULLPTR;
    value.base_path.size = 0u;
    value.import_callbacks = GOGGLES_FC_NULLPTR;
    return value;
}

static inline goggles_fc_chain_create_info_t goggles_fc_chain_create_info_init(GOGGLES_FC_NOARGS) {
    goggles_fc_chain_create_info_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_chain_create_info_t);
    value.target_format = VK_FORMAT_UNDEFINED;
    value.frames_in_flight = 1u;
    value.initial_stage_mask = GOGGLES_FC_STAGE_MASK_ALL;
    value.initial_prechain_resolution.width = 0u;
    value.initial_prechain_resolution.height = 0u;
    return value;
}

static inline goggles_fc_chain_target_info_t goggles_fc_chain_target_info_init(GOGGLES_FC_NOARGS) {
    goggles_fc_chain_target_info_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_chain_target_info_t);
    value.target_format = VK_FORMAT_UNDEFINED;
    return value;
}

static inline goggles_fc_record_info_vk_t goggles_fc_record_info_vk_init(GOGGLES_FC_NOARGS) {
    goggles_fc_record_info_vk_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_record_info_vk_t);
    value.command_buffer = VK_NULL_HANDLE;
    value.source_image = VK_NULL_HANDLE;
    value.source_view = VK_NULL_HANDLE;
    value.source_extent.width = 0u;
    value.source_extent.height = 0u;
    value.target_view = VK_NULL_HANDLE;
    value.target_extent.width = 0u;
    value.target_extent.height = 0u;
    value.frame_index = 0u;
    value.scale_mode = GOGGLES_FC_SCALE_MODE_STRETCH;
    value.integer_scale = 1u;
    return value;
}

static inline goggles_fc_log_message_t goggles_fc_log_message_init(GOGGLES_FC_NOARGS) {
    goggles_fc_log_message_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_log_message_t);
    value.level = GOGGLES_FC_LOG_LEVEL_INFO;
    value.domain.data = GOGGLES_FC_NULLPTR;
    value.domain.size = 0u;
    value.message.data = GOGGLES_FC_NULLPTR;
    value.message.size = 0u;
    return value;
}

static inline goggles_fc_control_info_t goggles_fc_control_info_init(GOGGLES_FC_NOARGS) {
    goggles_fc_control_info_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_control_info_t);
    value.index = 0u;
    value.stage = GOGGLES_FC_STAGE_EFFECT;
    value.name.data = GOGGLES_FC_NULLPTR;
    value.name.size = 0u;
    value.description.data = GOGGLES_FC_NULLPTR;
    value.description.size = 0u;
    value.current_value = 0.0f;
    value.default_value = 0.0f;
    value.min_value = 0.0f;
    value.max_value = 0.0f;
    value.step = 0.0f;
    return value;
}

static inline goggles_fc_program_source_info_t
goggles_fc_program_source_info_init(GOGGLES_FC_NOARGS) {
    goggles_fc_program_source_info_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_program_source_info_t);
    value.provenance = GOGGLES_FC_PROVENANCE_FILE;
    value.source_name.data = GOGGLES_FC_NULLPTR;
    value.source_name.size = 0u;
    value.source_path.data = GOGGLES_FC_NULLPTR;
    value.source_path.size = 0u;
    value.pass_count = 0u;
    return value;
}

static inline goggles_fc_program_report_t goggles_fc_program_report_init(GOGGLES_FC_NOARGS) {
    goggles_fc_program_report_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_program_report_t);
    value.shader_count = 0u;
    value.pass_count = 0u;
    value.texture_count = 0u;
    return value;
}

static inline goggles_fc_chain_report_t goggles_fc_chain_report_init(GOGGLES_FC_NOARGS) {
    goggles_fc_chain_report_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_chain_report_t);
    value.pass_count = 0u;
    value.frames_rendered = 0u;
    value.current_stage_mask = GOGGLES_FC_STAGE_MASK_ALL;
    return value;
}

static inline goggles_fc_chain_error_info_t goggles_fc_chain_error_info_init(GOGGLES_FC_NOARGS) {
    goggles_fc_chain_error_info_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_chain_error_info_t);
    value.status = GOGGLES_FC_STATUS_OK;
    value.vk_result = 0;
    value.subsystem_code = 0u;
    return value;
}

static inline goggles_fc_import_callbacks_t goggles_fc_import_callbacks_init(GOGGLES_FC_NOARGS) {
    goggles_fc_import_callbacks_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_import_callbacks_t);
    value.read_fn = GOGGLES_FC_NULLPTR;
    value.free_fn = GOGGLES_FC_NULLPTR;
    value.user_data = GOGGLES_FC_NULLPTR;
    return value;
}

static inline goggles_fc_diagnostic_policy_t goggles_fc_diagnostic_policy_init(GOGGLES_FC_NOARGS) {
    goggles_fc_diagnostic_policy_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_diagnostic_policy_t);
    value.capture_mode = GOGGLES_FC_DIAGNOSTIC_CAPTURE_STANDARD;
    value.activation_tier = GOGGLES_FC_DIAGNOSTIC_TIER_0;
    value.capture_frame_limit = 1u;
    return value;
}

static inline goggles_fc_diagnostic_summary_t
goggles_fc_diagnostic_summary_init(GOGGLES_FC_NOARGS) {
    goggles_fc_diagnostic_summary_t value;
    value.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_diagnostic_summary_t);
    value.session_active = false;
    value.debug_count = 0u;
    value.info_count = 0u;
    value.warning_count = 0u;
    value.error_count = 0u;
    value.current_frame = 0u;
    value.total_events = 0u;
    return value;
}

/* ── Process-global queries ──────────────────────────────────────────────── */

GOGGLES_FC_API uint32_t GOGGLES_FC_CALL goggles_fc_get_api_version(GOGGLES_FC_NOARGS);

GOGGLES_FC_API uint32_t GOGGLES_FC_CALL goggles_fc_get_abi_version(GOGGLES_FC_NOARGS);

GOGGLES_FC_API
goggles_fc_capability_flags_t GOGGLES_FC_CALL goggles_fc_get_capabilities(GOGGLES_FC_NOARGS);

GOGGLES_FC_API const char* GOGGLES_FC_CALL goggles_fc_status_string(goggles_fc_status_t status);

GOGGLES_FC_API bool GOGGLES_FC_CALL goggles_fc_is_success(goggles_fc_status_t status);

GOGGLES_FC_API bool GOGGLES_FC_CALL goggles_fc_is_error(goggles_fc_status_t status);

/* ── Instance lifecycle ──────────────────────────────────────────────────── */

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_instance_create(
    const goggles_fc_instance_create_info_t* create_info, goggles_fc_instance_t** out_instance);

GOGGLES_FC_API void GOGGLES_FC_CALL goggles_fc_instance_destroy(goggles_fc_instance_t* instance);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_instance_set_log_callback(
    goggles_fc_instance_t* instance, goggles_fc_log_callback_t log_callback, void* user_data);

/* ── Device lifecycle ────────────────────────────────────────────────────── */

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_device_create_vk(
    goggles_fc_instance_t* instance, const goggles_fc_vk_device_create_info_t* create_info,
    goggles_fc_device_t** out_device);

GOGGLES_FC_API void GOGGLES_FC_CALL goggles_fc_device_destroy(goggles_fc_device_t* device);

/* ── Program lifecycle ───────────────────────────────────────────────────── */

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_program_create(goggles_fc_device_t* device, const goggles_fc_preset_source_t* source,
                          goggles_fc_program_t** out_program);

GOGGLES_FC_API void GOGGLES_FC_CALL goggles_fc_program_destroy(goggles_fc_program_t* program);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_program_get_source_info(
    const goggles_fc_program_t* program, goggles_fc_program_source_info_t* out_source_info);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_program_get_report(
    const goggles_fc_program_t* program, goggles_fc_program_report_t* out_report);

/* ── Chain lifecycle ─────────────────────────────────────────────────────── */

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_create(
    goggles_fc_device_t* device, const goggles_fc_program_t* program,
    const goggles_fc_chain_create_info_t* create_info, goggles_fc_chain_t** out_chain);

GOGGLES_FC_API void GOGGLES_FC_CALL goggles_fc_chain_destroy(goggles_fc_chain_t* chain);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_bind_program(goggles_fc_chain_t* chain, const goggles_fc_program_t* program);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_clear(goggles_fc_chain_t* chain);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_resize(goggles_fc_chain_t* chain, const goggles_fc_extent_2d_t* new_source_extent);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_set_prechain_resolution(
    goggles_fc_chain_t* chain, const goggles_fc_extent_2d_t* resolution);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_set_stage_mask(goggles_fc_chain_t* chain, uint32_t stage_mask);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_retarget(
    goggles_fc_chain_t* chain, const goggles_fc_chain_target_info_t* target_info);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_record_vk(
    goggles_fc_chain_t* chain, const goggles_fc_record_info_vk_t* record_info);

/* ── Chain report / error queries ────────────────────────────────────────── */

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_get_report(const goggles_fc_chain_t* chain, goggles_fc_chain_report_t* out_report);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_get_last_error(
    const goggles_fc_chain_t* chain, goggles_fc_chain_error_info_t* out_error);

/* ── Chain control queries and mutation ───────────────────────────────────── */

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_get_control_count(const goggles_fc_chain_t* chain, uint32_t* out_count);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_get_control_info(
    const goggles_fc_chain_t* chain, uint32_t index, goggles_fc_control_info_t* out_control);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_find_control_index(const goggles_fc_chain_t* chain, uint32_t stage,
                                    goggles_fc_utf8_view_t name, uint32_t* out_index);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_set_control_value_f32(goggles_fc_chain_t* chain, uint32_t index, float value);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_set_control_value_f32_by_name(
    goggles_fc_chain_t* chain, uint32_t stage, goggles_fc_utf8_view_t name, float value);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_reset_control_value(goggles_fc_chain_t* chain, uint32_t index);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_reset_all_controls(goggles_fc_chain_t* chain);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_get_prechain_resolution(
    const goggles_fc_chain_t* chain, goggles_fc_extent_2d_t* out_resolution);

/* ── Chain diagnostics ───────────────────────────────────────────────────── */

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_create_diagnostic_session(
    goggles_fc_chain_t* chain, const goggles_fc_diagnostic_policy_t* policy);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_destroy_diagnostic_session(goggles_fc_chain_t* chain);

GOGGLES_FC_API goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_get_diagnostic_summary(
    const goggles_fc_chain_t* chain, goggles_fc_diagnostic_summary_t* out_summary);

#undef GOGGLES_FC_NOARGS
#undef GOGGLES_FC_NULLPTR

#ifdef __cplusplus
}
#endif

#endif /* GOGGLES_FILTER_CHAIN_H */
