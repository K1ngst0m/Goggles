## ADDED Requirements

### Requirement: Public Header and Export Surface
The filter-chain C API MUST be defined in a single public header named `include/goggles_filter_chain.h`. All public types and functions in ABI v1 MUST use the `goggles_chain_` prefix, all exported functions MUST use `GOGGLES_CHAIN_CALL`, and all symbols declared in the v1 header MUST be exported when `goggles_chain_abi_version()` returns `GOGGLES_CHAIN_ABI_VERSION`.

#### Scenario: Host links against ABI v1 library
- **GIVEN** a host compiles against `include/goggles_filter_chain.h`
- **WHEN** the host links against a library reporting ABI v1
- **THEN** every function symbol declared by the v1 header resolves without requiring an optional loader table

### Requirement: Version and Capability Negotiation
The API MUST expose `goggles_chain_api_version()`, `goggles_chain_abi_version()`, and `goggles_chain_capabilities_get(...)` for negotiation. `goggles_chain_api_version()` MUST return packed semantic version bits (`major << 22 | minor << 12 | patch`), `goggles_chain_abi_version()` MUST return ABI major, and capability flags MUST describe optional behavior only and MUST NOT indicate symbol presence.

#### Scenario: Host checks runtime support
- **GIVEN** a host initializes capability negotiation before creating a runtime
- **WHEN** it calls version and capability queries
- **THEN** it can determine ABI compatibility and optional feature availability without probing symbols dynamically

### Requirement: Fixed-Width Public Scalar Types
Enum-like public API scalars MUST use fixed-width `uint32_t` typedefs with named constants, including status, stage, stage mask, scale mode, and capability flags. The C ABI MUST keep these scalar widths stable across all v1.x releases.

#### Scenario: FFI binding validates scalar ABI
- **GIVEN** a language binding maps public scalar typedefs from the header
- **WHEN** the binding validates type widths at load time
- **THEN** each enum-like scalar maps to 32-bit unsigned storage and remains stable across v1.x

### Requirement: Runtime Lifecycle and State Semantics
`goggles_chain_t` MUST follow a deterministic state model: successful create enters `CREATED`, successful preset load enters or remains `READY`, and destroy transitions to `DEAD` by nulling the public handle. APIs that require an initialized preset (`record`, control listing, control mutation/reset) MUST return `GOGGLES_CHAIN_STATUS_NOT_INITIALIZED` when invoked before READY. `goggles_chain_destroy(...)` MUST be null-safe, idempotent, and return `GOGGLES_CHAIN_STATUS_OK` for `NULL`, `&NULL`, or live handles.

#### Scenario: Record called before preset load
- **GIVEN** a runtime that has been created but has never completed a preset load
- **WHEN** the host calls `goggles_chain_record_vk(...)`
- **THEN** the function returns `GOGGLES_CHAIN_STATUS_NOT_INITIALIZED` and the runtime remains usable

### Requirement: Error Model and Diagnostics Contract
All fallible APIs MUST return `goggles_chain_status_t` and MUST NOT surface exceptions as part of the public contract. `goggles_chain_status_to_string(...)` MUST return a stable static string and unknown status values MUST map to `"UNKNOWN_STATUS"`. Optional structured diagnostics MUST be queried through `goggles_chain_error_last_info_get(...)`; when unsupported, the API MUST return `GOGGLES_CHAIN_STATUS_NOT_SUPPORTED`.

#### Scenario: Host inspects unknown status code
- **GIVEN** a status value outside known v1 constants
- **WHEN** the host calls `goggles_chain_status_to_string(...)`
- **THEN** the function returns the stable token `"UNKNOWN_STATUS"` without heap allocation

### Requirement: Out-Parameter Failure Semantics
On success, output parameters MUST be fully initialized as documented. On failure, outputs MUST remain unchanged except creator/list APIs, which MUST set owned outputs to `NULL` when an output pointer is provided (`goggles_chain_create_*` sets `*out_chain = NULL`; `goggles_chain_control_list*` sets `*out_snapshot = NULL`).

#### Scenario: Create fails validation
- **GIVEN** a non-null `out_chain` initialized to a sentinel value
- **WHEN** `goggles_chain_create_vk(...)` returns failure
- **THEN** `*out_chain` is set to `NULL`

### Requirement: Struct Size Extensibility Contract
Each extensible public struct input/output gated by `struct_size` MUST reject `struct_size < sizeof(v1_type)` with `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`. For `struct_size >= sizeof(v1_type)`, implementations MUST read/write only the known v1 prefix and MUST leave unknown output tail bytes untouched.

#### Scenario: Future-tail struct passed to v1 runtime
- **GIVEN** a caller passes a larger struct with v1-compatible prefix and extra tail bytes
- **WHEN** the runtime processes the call
- **THEN** behavior is based only on the v1 prefix and unknown tail bytes are not interpreted

### Requirement: Vulkan Create Input Validation
Create APIs MUST validate required Vulkan context and create-info inputs, including non-null required pointers, `num_sync_indices >= 1`, `num_sync_indices <= max_sync_indices`, required non-empty shader directory input, and positive `initial_prechain_resolution` dimensions. Invalid inputs MUST return `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`.

#### Scenario: Invalid sync index count
- **GIVEN** create info with `num_sync_indices` equal to zero
- **WHEN** the host calls `goggles_chain_create_vk(...)`
- **THEN** creation fails with `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`

### Requirement: UTF-8 Path and Length-Based API Contract
The API MUST provide both C-string and length-based variants for create and preset loading (`*_ex` variants). `_ex` path inputs MUST accept explicit byte lengths, MUST reject invalid UTF-8 and embedded NUL bytes, and MUST require non-null pointers when length is non-zero. `goggles_chain_preset_load(...)` MUST be equivalent to `_ex(path, strlen(path))` for valid NUL-terminated UTF-8 input.

#### Scenario: Preset load with malformed UTF-8
- **GIVEN** a byte span containing invalid UTF-8 for preset path input
- **WHEN** the host calls `goggles_chain_preset_load_ex(...)`
- **THEN** the function returns `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT` without mutating active runtime state

### Requirement: Frame Recording Contract and Performance Bounds
`goggles_chain_record_vk(...)` MUST record commands only and MUST NOT submit or present. The caller MUST provide a command buffer already in recording state, valid source/target views, and `frame_index < num_sync_indices`. On `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`, no commands MUST be recorded. In v1 hot path, record MUST perform no heap allocation, file I/O, shader compilation, or blocking waits.

#### Scenario: Invalid frame index
- **GIVEN** a runtime created with `num_sync_indices = 2`
- **WHEN** the host calls `goggles_chain_record_vk(...)` with `frame_index = 2`
- **THEN** the function returns `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT` and records no commands

### Requirement: Stage Model and Policy Contract
The v1 stage model MUST include `prechain`, `effect`, and `postchain` as first-class stage values and masks. `goggles_chain_stage_policy_set(...)` MUST reject unknown stage bits and zero mask with `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`. Runtime execution order MUST remain `prechain -> effect -> postchain`.

#### Scenario: Unknown stage bit in policy
- **GIVEN** a stage policy mask containing bits outside known v1 stage-mask constants
- **WHEN** the host calls `goggles_chain_stage_policy_set(...)`
- **THEN** the function returns `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`

### Requirement: Prechain Resolution Contract
`goggles_chain_prechain_resolution_set(...)` and `goggles_chain_prechain_resolution_get(...)` MUST be valid in both `CREATED` and `READY` states. Resolution values MUST require positive width and height and invalid extents MUST return `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`.

#### Scenario: Zero-height prechain resolution
- **GIVEN** a live runtime instance
- **WHEN** the host sets prechain resolution with `height = 0`
- **THEN** the API returns `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`

### Requirement: Snapshot-Based Control Listing Contract
Control exposure MUST be snapshot-based through `goggles_chain_control_snapshot_t` ownership APIs. `goggles_chain_control_list(...)` ordering MUST be deterministic as prechain, then effect, then postchain. `goggles_chain_control_list_stage(..., GOGGLES_CHAIN_STAGE_POSTCHAIN, ...)` MUST return a valid empty snapshot with `GOGGLES_CHAIN_STATUS_OK` in v1. Snapshot getters for null snapshot MUST return `0` count and `NULL` data.

#### Scenario: Postchain stage list in v1
- **GIVEN** a READY runtime with no postchain controls
- **WHEN** the host requests `goggles_chain_control_list_stage(..., GOGGLES_CHAIN_STAGE_POSTCHAIN, ...)`
- **THEN** the API succeeds and returns an owned snapshot whose count is zero

### Requirement: Control Descriptor Lifetime and Mutation Semantics
Control descriptors MUST expose stable `control_id` mutation keys and borrowed string/data pointers valid only while the owning snapshot is alive. `goggles_chain_control_set_value(...)` MUST clamp finite values to descriptor bounds before apply, MUST reject non-finite values (`NaN`, `+Inf`, `-Inf`) with `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`, and MUST leave control state unchanged on non-finite rejection.

#### Scenario: Non-finite control input
- **GIVEN** a READY runtime and a valid control identifier
- **WHEN** the host calls `goggles_chain_control_set_value(...)` with `NaN`
- **THEN** the API returns `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT` and does not modify the control value

### Requirement: Ownership, Handle Provenance, and Pointer Retention
The runtime MUST NOT retain pointers to caller-provided input structs or path buffers after calls return. Vulkan handles in create context (`VkDevice`, `VkPhysicalDevice`, `VkQueue`) MUST remain valid for runtime lifetime, while record-time Vulkan handles are borrowed for call duration only. Non-null handles passed to API entrypoints MUST originate from corresponding successful create/list APIs in the same process and remain live.

#### Scenario: Caller frees path buffer after call
- **GIVEN** a host passes valid path bytes to an API call
- **WHEN** the call returns
- **THEN** the host may free or mutate the path buffer without affecting runtime memory safety

### Requirement: Threading and Reentrancy Contract
The API MUST provide no internal synchronization for a single `goggles_chain_t` instance, and callers MUST externally serialize all calls (including getters/listing) per runtime instance. Calls on different runtime instances are permitted to proceed concurrently. Global version/capability/status-string APIs MUST be thread-safe and reentrant.

#### Scenario: Concurrent usage across independent runtimes
- **GIVEN** two distinct runtime instances
- **WHEN** two threads call APIs on different instances concurrently
- **THEN** concurrent usage is permitted as long as each instance is internally serialized by its caller

### Requirement: Compatibility and Evolution Policy for v1.x
Across v1.x, patch releases MUST avoid source/ABI breaking changes and minor releases MUST be additive only. Incompatible layout/signature/calling-convention changes MUST require an ABI major bump. Deprecated declarations MUST use `GOGGLES_CHAIN_DEPRECATED(...)` when deprecation is introduced, and capability flags MUST gate optional behavior rather than symbol availability.

#### Scenario: Additive minor release behavior
- **GIVEN** a host compiled against v1.0 header
- **WHEN** it links against a v1.x minor release
- **THEN** existing v1.0 symbols and contracts continue to work without source-breaking changes
