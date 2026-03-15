/**
 * @file main.c
 * Installed C-consumer validation for the goggles-filter-chain package.
 *
 * This is a pure C11 program that validates the public C ABI surface by linking
 * against the INSTALLED GogglesFilterChain package via find_package().
 *
 * It includes ONLY <goggles_filter_chain.h> — no Goggles-private headers, no
 * C++ headers, no shader_dir inputs.
 *
 * Usage:
 *   c_api_consumer                      — runs version/status/instance tests only
 *   c_api_consumer <preset_path>        — also tests file-source program creation
 *
 * Exit: 0 on success, non-zero on first failure.
 */

#include <goggles_filter_chain.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static int g_test_count = 0;
static int g_pass_count = 0;

#define CHECK(expr, msg)                                                                           \
    do {                                                                                           \
        ++g_test_count;                                                                            \
        if (!(expr)) {                                                                             \
            printf("FAIL [%d]: %s  (%s:%d)\n", g_test_count, (msg), __FILE__, __LINE__);           \
            return 1;                                                                              \
        }                                                                                          \
        ++g_pass_count;                                                                            \
        printf("PASS [%d]: %s\n", g_test_count, (msg));                                            \
    } while (0)

/* ── Log callback used for memory-source flow ────────────────────────────── */

static int g_log_call_count = 0;

static void GOGGLES_FC_CALL test_log_callback(const goggles_fc_log_message_t* message,
                                              void* user_data) {
    (void)message;
    (void)user_data;
    ++g_log_call_count;
}

/* ── Import callbacks for memory-source flow ─────────────────────────────── */

/* Trivial import reader that always fails — we only test that the struct is
   usable from C, not that the import actually resolves anything.  A real
   consumer would read relative shader files here. */
static goggles_fc_import_result_t GOGGLES_FC_CALL
dummy_import_read(goggles_fc_utf8_view_t relative_path, void** out_bytes, size_t* out_byte_count,
                  void* user_data) {
    (void)relative_path;
    (void)out_bytes;
    (void)out_byte_count;
    (void)user_data;
    return GOGGLES_FC_STATUS_NOT_FOUND;
}

static void GOGGLES_FC_CALL dummy_import_free(void* bytes, size_t byte_count, void* user_data) {
    (void)bytes;
    (void)byte_count;
    (void)user_data;
}

/* ── Test: version and capability queries ────────────────────────────────── */

static int test_version_and_capabilities(void) {
    printf("\n--- Version & capability queries ---\n");

    const uint32_t api_ver = goggles_fc_get_api_version();
    CHECK(api_ver == GOGGLES_FC_API_VERSION,
          "goggles_fc_get_api_version matches GOGGLES_FC_API_VERSION");

    const uint32_t abi_ver = goggles_fc_get_abi_version();
    CHECK(abi_ver == GOGGLES_FC_ABI_VERSION,
          "goggles_fc_get_abi_version matches GOGGLES_FC_ABI_VERSION");

    const goggles_fc_capability_flags_t caps = goggles_fc_get_capabilities();
    CHECK((caps & GOGGLES_FC_CAPABILITY_VULKAN) != 0u, "capabilities include VULKAN");
    CHECK((caps & GOGGLES_FC_CAPABILITY_FILE_SOURCE) != 0u, "capabilities include FILE_SOURCE");
    CHECK((caps & GOGGLES_FC_CAPABILITY_MEMORY_SOURCE) != 0u, "capabilities include MEMORY_SOURCE");
    CHECK((caps & GOGGLES_FC_CAPABILITY_LOG_CALLBACK) != 0u, "capabilities include LOG_CALLBACK");

    return 0;
}

/* ── Test: status helpers ────────────────────────────────────────────────── */

static int test_status_helpers(void) {
    printf("\n--- Status helpers ---\n");

    /* goggles_fc_status_string returns non-null for all known codes */
    uint32_t code;
    for (code = 0u; code <= 11u; ++code) {
        const char* str = goggles_fc_status_string((goggles_fc_status_t)code);
        CHECK(str != NULL, "status_string non-null for known code");
        CHECK(strlen(str) > 0u, "status_string non-empty for known code");
    }

    /* is_success / is_error for OK */
    CHECK(goggles_fc_is_success(GOGGLES_FC_STATUS_OK) == true, "is_success(OK) is true");
    CHECK(goggles_fc_is_error(GOGGLES_FC_STATUS_OK) == false, "is_error(OK) is false");

    /* is_error is true for all error codes 1-11 */
    for (code = 1u; code <= 11u; ++code) {
        CHECK(goggles_fc_is_error((goggles_fc_status_t)code) == true,
              "is_error(error_code) is true");
        CHECK(goggles_fc_is_success((goggles_fc_status_t)code) == false,
              "is_success(error_code) is false");
    }

    return 0;
}

/* ── Test: struct initializers from C ────────────────────────────────────── */

static int test_struct_initializers(void) {
    printf("\n--- Struct initializers ---\n");

    /* Instance create info */
    goggles_fc_instance_create_info_t ici = goggles_fc_instance_create_info_init();
    CHECK(ici.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_instance_create_info_t),
          "instance_create_info_init sets struct_size");
    CHECK(ici.log_callback == NULL, "instance_create_info_init nulls log_callback");
    CHECK(ici.log_user_data == NULL, "instance_create_info_init nulls log_user_data");

    /* Preset source */
    goggles_fc_preset_source_t ps = goggles_fc_preset_source_init();
    CHECK(ps.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_preset_source_t),
          "preset_source_init sets struct_size");
    CHECK(ps.kind == GOGGLES_FC_PRESET_SOURCE_FILE, "preset_source_init defaults to FILE kind");
    CHECK(ps.bytes == NULL, "preset_source_init nulls bytes");
    CHECK(ps.path.data == NULL, "preset_source_init nulls path.data");

    /* Chain create info */
    goggles_fc_chain_create_info_t cci = goggles_fc_chain_create_info_init();
    CHECK(cci.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_chain_create_info_t),
          "chain_create_info_init sets struct_size");
    CHECK(cci.frames_in_flight == 1u, "chain_create_info_init defaults frames_in_flight to 1");
    CHECK(cci.initial_stage_mask == GOGGLES_FC_STAGE_MASK_ALL,
          "chain_create_info_init defaults stage_mask to ALL");

    /* VK device create info */
    goggles_fc_vk_device_create_info_t vdi = goggles_fc_vk_device_create_info_init();
    CHECK(vdi.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_vk_device_create_info_t),
          "vk_device_create_info_init sets struct_size");
    CHECK(vdi.cache_dir.data == NULL, "vk_device_create_info_init nulls cache_dir");

    /* Import callbacks */
    goggles_fc_import_callbacks_t ic = goggles_fc_import_callbacks_init();
    CHECK(ic.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_import_callbacks_t),
          "import_callbacks_init sets struct_size");
    CHECK(ic.read_fn == NULL, "import_callbacks_init nulls read_fn");
    CHECK(ic.free_fn == NULL, "import_callbacks_init nulls free_fn");
    CHECK(ic.user_data == NULL, "import_callbacks_init nulls user_data");

    /* Log message */
    goggles_fc_log_message_t lm = goggles_fc_log_message_init();
    CHECK(lm.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_log_message_t),
          "log_message_init sets struct_size");
    CHECK(lm.level == GOGGLES_FC_LOG_LEVEL_INFO, "log_message_init defaults level to INFO");

    /* Record info */
    goggles_fc_record_info_vk_t ri = goggles_fc_record_info_vk_init();
    CHECK(ri.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_record_info_vk_t),
          "record_info_vk_init sets struct_size");
    CHECK(ri.scale_mode == GOGGLES_FC_SCALE_MODE_STRETCH,
          "record_info_vk_init defaults scale_mode to STRETCH");

    /* Control info */
    goggles_fc_control_info_t ci = goggles_fc_control_info_init();
    CHECK(ci.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_control_info_t),
          "control_info_init sets struct_size");
    CHECK(ci.stage == GOGGLES_FC_STAGE_EFFECT, "control_info_init defaults stage to EFFECT");

    /* Chain target info */
    goggles_fc_chain_target_info_t cti = goggles_fc_chain_target_info_init();
    CHECK(cti.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_chain_target_info_t),
          "chain_target_info_init sets struct_size");

    /* Program source info */
    goggles_fc_program_source_info_t psi = goggles_fc_program_source_info_init();
    CHECK(psi.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_program_source_info_t),
          "program_source_info_init sets struct_size");
    CHECK(psi.provenance == GOGGLES_FC_PROVENANCE_FILE,
          "program_source_info_init defaults provenance to FILE");

    /* Program report */
    goggles_fc_program_report_t pr = goggles_fc_program_report_init();
    CHECK(pr.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_program_report_t),
          "program_report_init sets struct_size");

    /* Chain report */
    goggles_fc_chain_report_t cr = goggles_fc_chain_report_init();
    CHECK(cr.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_chain_report_t),
          "chain_report_init sets struct_size");
    CHECK(cr.current_stage_mask == GOGGLES_FC_STAGE_MASK_ALL,
          "chain_report_init defaults stage_mask to ALL");

    /* Chain error info */
    goggles_fc_chain_error_info_t cei = goggles_fc_chain_error_info_init();
    CHECK(cei.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_chain_error_info_t),
          "chain_error_info_init sets struct_size");
    CHECK(cei.status == GOGGLES_FC_STATUS_OK, "chain_error_info_init defaults status to OK");

    return 0;
}

/* ── Test: instance create/destroy lifecycle ─────────────────────────────── */

static int test_instance_lifecycle(void) {
    printf("\n--- Instance lifecycle ---\n");

    goggles_fc_instance_create_info_t info = goggles_fc_instance_create_info_init();
    goggles_fc_instance_t* instance = NULL;

    goggles_fc_status_t status = goggles_fc_instance_create(&info, &instance);
    CHECK(status == GOGGLES_FC_STATUS_OK, "instance_create returns OK");
    CHECK(instance != NULL, "instance_create produces non-null handle");

    /* Set and clear a log callback */
    status = goggles_fc_instance_set_log_callback(instance, test_log_callback, NULL);
    CHECK(status == GOGGLES_FC_STATUS_OK, "set_log_callback succeeds");

    status = goggles_fc_instance_set_log_callback(instance, NULL, NULL);
    CHECK(status == GOGGLES_FC_STATUS_OK, "clear log callback succeeds");

    /* Destroy */
    goggles_fc_instance_destroy(instance);
    printf("PASS [%d]: instance_destroy does not crash\n", ++g_test_count);
    ++g_pass_count;

    /* Destroy null is safe */
    goggles_fc_instance_destroy(NULL);
    printf("PASS [%d]: instance_destroy(NULL) is safe\n", ++g_test_count);
    ++g_pass_count;

    return 0;
}

/* ── Test: additive control identity helpers are visible from C ───────────── */

static int test_control_identity_helpers(void) {
    printf("\n--- Control identity helpers ---\n");

    const char control_name[] = "CONTROL_A";
    const goggles_fc_utf8_view_t name = {
        .data = control_name,
        .size = sizeof(control_name) - 1u,
    };
    uint32_t index = 0u;

    CHECK(goggles_fc_chain_find_control_index(NULL, GOGGLES_FC_STAGE_EFFECT, name, &index) ==
              GOGGLES_FC_STATUS_INVALID_ARGUMENT,
          "find_control_index is exported and validates null chain");
    CHECK(goggles_fc_chain_set_control_value_f32_by_name(
              NULL, GOGGLES_FC_STAGE_EFFECT, name, 0.5f) == GOGGLES_FC_STATUS_INVALID_ARGUMENT,
          "set_control_value_f32_by_name is exported and validates null chain");

    return 0;
}

/* ── Test: file-source program creation flow ─────────────────────────────── */

static int test_file_source_flow(const char* preset_path) {
    printf("\n--- File-source flow (preset: %s) ---\n", preset_path);

    /* Create instance */
    goggles_fc_instance_create_info_t inst_info = goggles_fc_instance_create_info_init();
    inst_info.log_callback = test_log_callback;
    goggles_fc_instance_t* instance = NULL;
    goggles_fc_status_t status = goggles_fc_instance_create(&inst_info, &instance);
    CHECK(status == GOGGLES_FC_STATUS_OK, "file-source: instance_create OK");

    /* Prepare file preset source */
    goggles_fc_preset_source_t src = goggles_fc_preset_source_init();
    src.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    src.path.data = preset_path;
    src.path.size = strlen(preset_path);
    src.source_name.data = "file-consumer-test";
    src.source_name.size = strlen("file-consumer-test");

    /* Program creation requires a device, which requires Vulkan handles.
       For the installed consumer test we cannot assume Vulkan is available,
       so we verify the API is callable and returns a sensible error when
       no device is provided (the program_create function validates its
       device argument before touching the preset source). */

    /* Verify null-argument validation */
    goggles_fc_program_t* prog = NULL;
    status = goggles_fc_program_create(NULL, &src, &prog);
    CHECK(status == GOGGLES_FC_STATUS_INVALID_ARGUMENT,
          "file-source: program_create(NULL device) returns INVALID_ARGUMENT");
    CHECK(prog == NULL, "file-source: program_create(NULL device) leaves out_program NULL");

    /* Verify source info query with null program returns error */
    goggles_fc_program_source_info_t si = goggles_fc_program_source_info_init();
    status = goggles_fc_program_get_source_info(NULL, &si);
    CHECK(status == GOGGLES_FC_STATUS_INVALID_ARGUMENT,
          "file-source: get_source_info(NULL program) returns INVALID_ARGUMENT");

    /* Verify program report query with null program returns error */
    goggles_fc_program_report_t rpt = goggles_fc_program_report_init();
    status = goggles_fc_program_get_report(NULL, &rpt);
    CHECK(status == GOGGLES_FC_STATUS_INVALID_ARGUMENT,
          "file-source: get_report(NULL program) returns INVALID_ARGUMENT");

    /* Destroy null program is safe */
    goggles_fc_program_destroy(NULL);
    printf("PASS [%d]: file-source: program_destroy(NULL) is safe\n", ++g_test_count);
    ++g_pass_count;

    goggles_fc_instance_destroy(instance);
    return 0;
}

/* ── Test: memory-source program creation flow ───────────────────────────── */

static int test_memory_source_flow(void) {
    printf("\n--- Memory-source flow ---\n");

    /* Create instance */
    goggles_fc_instance_create_info_t inst_info = goggles_fc_instance_create_info_init();
    inst_info.log_callback = test_log_callback;
    goggles_fc_instance_t* instance = NULL;
    goggles_fc_status_t status = goggles_fc_instance_create(&inst_info, &instance);
    CHECK(status == GOGGLES_FC_STATUS_OK, "memory-source: instance_create OK");

    /* Set up a trivial preset string in memory */
    static const char preset_text[] = "shaders = 1\nshader0 = test.slang\n";
    const size_t preset_len = sizeof(preset_text) - 1u;

    /* Prepare memory preset source */
    goggles_fc_preset_source_t src = goggles_fc_preset_source_init();
    src.kind = GOGGLES_FC_PRESET_SOURCE_MEMORY;
    src.source_name.data = "memory://test-consumer";
    src.source_name.size = strlen("memory://test-consumer");
    src.bytes = preset_text;
    src.byte_count = preset_len;

    /* Set up import callbacks (C struct usability from C code) */
    goggles_fc_import_callbacks_t callbacks = goggles_fc_import_callbacks_init();
    callbacks.read_fn = dummy_import_read;
    callbacks.free_fn = dummy_import_free;
    callbacks.user_data = NULL;
    src.import_callbacks = &callbacks;

    /* Like the file-source flow, full program creation requires a Vulkan
       device. We verify the API is callable from C with memory-source
       parameters and validate null-argument handling. */

    goggles_fc_program_t* prog = NULL;
    status = goggles_fc_program_create(NULL, &src, &prog);
    CHECK(status == GOGGLES_FC_STATUS_INVALID_ARGUMENT,
          "memory-source: program_create(NULL device) returns INVALID_ARGUMENT");
    CHECK(prog == NULL, "memory-source: program_create(NULL device) leaves out_program NULL");

    /* Verify the import callbacks struct is well-formed from C */
    CHECK(callbacks.struct_size == GOGGLES_FC_STRUCT_SIZE(goggles_fc_import_callbacks_t),
          "memory-source: import_callbacks struct_size correct");
    CHECK(callbacks.read_fn == dummy_import_read,
          "memory-source: import_callbacks.read_fn preserved");
    CHECK(callbacks.free_fn == dummy_import_free,
          "memory-source: import_callbacks.free_fn preserved");

    goggles_fc_instance_destroy(instance);
    return 0;
}

/* ── Test: version macro arithmetic ──────────────────────────────────────── */

static int test_version_macros(void) {
    printf("\n--- Version macro arithmetic ---\n");

    /* GOGGLES_FC_MAKE_VERSION round-trips */
    const uint32_t v = GOGGLES_FC_MAKE_VERSION(1u, 0u, 0u);
    CHECK(v == GOGGLES_FC_API_VERSION, "MAKE_VERSION(1,0,0) equals API_VERSION");

    /* Verify major/minor/patch extraction works (bit packing) */
    const uint32_t v2 = GOGGLES_FC_MAKE_VERSION(2u, 3u, 7u);
    const uint32_t major = (v2 >> 22) & 0x3ffu;
    const uint32_t minor = (v2 >> 12) & 0x3ffu;
    const uint32_t patch = v2 & 0xfffu;
    CHECK(major == 2u, "MAKE_VERSION major extraction");
    CHECK(minor == 3u, "MAKE_VERSION minor extraction");
    CHECK(patch == 7u, "MAKE_VERSION patch extraction");

    return 0;
}

/* ── Test: constant value contracts ──────────────────────────────────────── */

static int test_constant_values(void) {
    printf("\n--- Constant value contracts ---\n");

    /* Stage identifiers */
    CHECK(GOGGLES_FC_STAGE_PRECHAIN == 0u, "STAGE_PRECHAIN == 0");
    CHECK(GOGGLES_FC_STAGE_EFFECT == 1u, "STAGE_EFFECT == 1");
    CHECK(GOGGLES_FC_STAGE_POSTCHAIN == 2u, "STAGE_POSTCHAIN == 2");

    /* Stage masks */
    CHECK(GOGGLES_FC_STAGE_MASK_PRECHAIN == 1u, "STAGE_MASK_PRECHAIN == 1");
    CHECK(GOGGLES_FC_STAGE_MASK_EFFECT == 2u, "STAGE_MASK_EFFECT == 2");
    CHECK(GOGGLES_FC_STAGE_MASK_POSTCHAIN == 4u, "STAGE_MASK_POSTCHAIN == 4");
    CHECK(GOGGLES_FC_STAGE_MASK_ALL == 7u, "STAGE_MASK_ALL == 7");

    /* Scale modes */
    CHECK(GOGGLES_FC_SCALE_MODE_STRETCH == 0u, "SCALE_MODE_STRETCH == 0");
    CHECK(GOGGLES_FC_SCALE_MODE_FIT == 1u, "SCALE_MODE_FIT == 1");
    CHECK(GOGGLES_FC_SCALE_MODE_INTEGER == 2u, "SCALE_MODE_INTEGER == 2");

    /* Provenance */
    CHECK(GOGGLES_FC_PROVENANCE_FILE == 0u, "PROVENANCE_FILE == 0");
    CHECK(GOGGLES_FC_PROVENANCE_MEMORY == 1u, "PROVENANCE_MEMORY == 1");

    /* Preset source kinds */
    CHECK(GOGGLES_FC_PRESET_SOURCE_FILE == 0u, "PRESET_SOURCE_FILE == 0");
    CHECK(GOGGLES_FC_PRESET_SOURCE_MEMORY == 1u, "PRESET_SOURCE_MEMORY == 1");

    /* Log levels are ordered */
    CHECK(GOGGLES_FC_LOG_LEVEL_TRACE < GOGGLES_FC_LOG_LEVEL_DEBUG, "TRACE < DEBUG");
    CHECK(GOGGLES_FC_LOG_LEVEL_DEBUG < GOGGLES_FC_LOG_LEVEL_INFO, "DEBUG < INFO");
    CHECK(GOGGLES_FC_LOG_LEVEL_INFO < GOGGLES_FC_LOG_LEVEL_WARN, "INFO < WARN");
    CHECK(GOGGLES_FC_LOG_LEVEL_WARN < GOGGLES_FC_LOG_LEVEL_ERROR, "WARN < ERROR");
    CHECK(GOGGLES_FC_LOG_LEVEL_ERROR < GOGGLES_FC_LOG_LEVEL_CRITICAL, "ERROR < CRITICAL");

    return 0;
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char* argv[]) {
    printf("=== goggles-filter-chain C consumer validation ===\n");

    if (test_version_and_capabilities() != 0)
        return 1;
    if (test_status_helpers() != 0)
        return 1;
    if (test_struct_initializers() != 0)
        return 1;
    if (test_instance_lifecycle() != 0)
        return 1;
    if (test_control_identity_helpers() != 0)
        return 1;
    if (test_version_macros() != 0)
        return 1;
    if (test_constant_values() != 0)
        return 1;

    /* Memory-source flow — always runs (no external file dependency) */
    if (test_memory_source_flow() != 0)
        return 1;

    /* File-source flow — requires a preset file path */
    const char* preset_path = NULL;
    if (argc > 1) {
        preset_path = argv[1];
    } else {
        /* Fall back to environment variable */
        preset_path = getenv("GOGGLES_FC_TEST_PRESET");
    }

    if (preset_path != NULL && strlen(preset_path) > 0u) {
        if (test_file_source_flow(preset_path) != 0)
            return 1;
    } else {
        printf("\nSKIP: file-source flow (no preset path provided via argv[1] "
               "or GOGGLES_FC_TEST_PRESET env var)\n");
    }

    printf("\n=== %d/%d tests passed ===\n", g_pass_count, g_test_count);
    return 0;
}
