# filter-chain-c-api Specification

## Purpose
Defines the current standalone filter-chain C ABI: its canonical installed C entrypoint, exported symbol naming, opaque-handle lifecycle, version/capability queries, Vulkan-facing program/chain operations, and caller-visible diagnostics/control contracts.

## Requirements
### Requirement: Public ABI naming and export surface
The public C ABI MUST use `goggles/filter_chain.h` as its canonical installed entrypoint. It MUST use the `goggles_fc_` symbol prefix and `GOGGLES_FC_` macro/constant prefix. Exported functions MUST use `GOGGLES_FC_API` and `GOGGLES_FC_CALL`, and the header-level ABI identity MUST be expressed through `GOGGLES_FC_API_VERSION` and `GOGGLES_FC_ABI_VERSION`.

#### Scenario: Consumer links against the public ABI
- **GIVEN** a consumer compiles against `goggles/filter_chain.h`
- **WHEN** it links to a library reporting the same ABI major
- **THEN** the declared `goggles_fc_*` symbols SHALL be the supported callable ABI surface
- **AND** the consumer SHALL NOT rely on legacy `goggles_chain_*` naming

### Requirement: Global query helpers
The ABI MUST expose process-global query helpers `goggles_fc_get_api_version()`, `goggles_fc_get_abi_version()`, `goggles_fc_get_capabilities()`, `goggles_fc_status_string(...)`, `goggles_fc_is_success(...)`, and `goggles_fc_is_error(...)`. The packed API version MUST use `major << 22 | minor << 12 | patch` bit packing.

#### Scenario: Consumer checks runtime support
- **GIVEN** a consumer needs compatibility and feature negotiation before creating runtime objects
- **WHEN** it calls the global query helpers
- **THEN** it SHALL be able to determine ABI compatibility, optional capability flags, and human-readable status text without probing object handles first

### Requirement: Stable scalar constants and fixed-width typedefs
Status, log-level, capability, and preset-source-kind public scalar typedefs MUST remain 32-bit unsigned values. Public constants for status codes, capabilities, log levels, preset-source kinds, scale modes, stage identifiers, stage masks, and provenance kinds MUST remain expressible as fixed-width integer ABI values.

#### Scenario: FFI binding validates scalar widths
- **GIVEN** a foreign-language binding maps the public scalar typedefs
- **WHEN** it validates the ABI widths declared by the header
- **THEN** the mapped scalar storage SHALL be 32-bit unsigned for those typedef families

### Requirement: Four opaque-handle lifecycle model
The ABI MUST model runtime state through four opaque handles: `goggles_fc_instance_t`, `goggles_fc_device_t`, `goggles_fc_program_t`, and `goggles_fc_chain_t`. Creation APIs MUST return handles through out-parameters, and destroy APIs for those handles MUST be void-returning object destructors rather than status-returning pointer-to-pointer teardown APIs.

#### Scenario: Consumer owns distinct runtime objects
- **GIVEN** a consumer creates an instance, Vulkan device binding, preset program, and executable chain
- **WHEN** it destroys them through the public ABI
- **THEN** each object kind SHALL have its own dedicated destroy function
- **AND** the ABI SHALL NOT collapse those lifetimes into a single `goggles_chain_t` runtime model

### Requirement: Struct-based call contract and initialization helpers
The ABI MUST expose POD structs for UTF-8 views, extents, log messages, instance creation, Vulkan device creation, import callbacks, preset sources, chain creation, chain retargeting, Vulkan record info, control info, program source info, program report, chain report, chain error info, and diagnostic summary. Each struct family with a `struct_size` field MUST support `GOGGLES_FC_STRUCT_SIZE(...)`-based initialization, and the header MUST provide inline `*_init()` helpers for the public struct families that callers are expected to initialize.

#### Scenario: Caller prepares a public input struct
- **GIVEN** a caller is about to invoke a struct-based ABI entrypoint
- **WHEN** it initializes the struct with the corresponding `*_init()` helper or equivalent `struct_size`
- **THEN** the call SHALL use the documented v1 struct layout prefix
- **AND** the caller SHALL NOT need legacy `_ex` path-variant APIs to opt into extensible struct inputs

### Requirement: Instance lifecycle and logging callback contract
The ABI MUST expose `goggles_fc_instance_create(...)`, `goggles_fc_instance_destroy(...)`, and `goggles_fc_instance_set_log_callback(...)`. Instance creation MUST accept optional log callback configuration through `goggles_fc_instance_create_info_t`, and the callback contract MUST deliver `goggles_fc_log_message_t` with log level plus UTF-8 domain and message views.

#### Scenario: Host installs or replaces a log callback
- **GIVEN** a live instance handle
- **WHEN** the host configures a log callback at create time or later through `goggles_fc_instance_set_log_callback(...)`
- **THEN** the callback contract SHALL use `goggles_fc_log_message_t`
- **AND** the public ABI SHALL expose log levels through `GOGGLES_FC_LOG_LEVEL_*` constants

### Requirement: Vulkan device binding contract
The ABI MUST expose `goggles_fc_device_create_vk(...)` and `goggles_fc_device_destroy(...)`. Vulkan device creation MUST accept `goggles_fc_vk_device_create_info_t` containing physical device, logical device, graphics queue, graphics queue family index, and optional cache-directory UTF-8 view.

#### Scenario: Consumer binds an existing Vulkan device
- **GIVEN** a live filter-chain instance and an existing Vulkan device context
- **WHEN** the consumer creates a filter-chain device binding
- **THEN** the public ABI SHALL accept Vulkan handles and queue metadata through `goggles_fc_vk_device_create_info_t`

### Requirement: Program creation uses preset-source descriptors
The ABI MUST expose `goggles_fc_program_create(...)`, `goggles_fc_program_destroy(...)`, `goggles_fc_program_get_source_info(...)`, and `goggles_fc_program_get_report(...)`. Program creation MUST consume a `goggles_fc_preset_source_t` descriptor rather than legacy preset-load functions. The preset source contract MUST support file-backed and memory-backed sources, plus import callbacks for memory-backed include resolution.

#### Scenario: Consumer creates a program from memory-backed preset bytes
- **GIVEN** a consumer has preset bytes and import callbacks for dependent resources
- **WHEN** it creates a program with `kind = GOGGLES_FC_PRESET_SOURCE_MEMORY`
- **THEN** the ABI SHALL accept the program source through `goggles_fc_preset_source_t`
- **AND** the public contract SHALL use the import-callback struct rather than `preset_load[_ex]` APIs

### Requirement: File-source passthrough contract
For `GOGGLES_FC_PRESET_SOURCE_FILE`, a non-null `path.data` with `path.size == 0` MUST represent passthrough mode: no preset file is loaded and the runtime uses a built-in single-pass blit pipeline. A null `path.data` for file sources MUST be rejected as invalid input.

#### Scenario: Caller requests passthrough mode
- **GIVEN** a file-source preset descriptor with a non-null path pointer and zero path length
- **WHEN** the caller creates a program from that descriptor
- **THEN** the public contract SHALL treat the request as passthrough mode
- **AND** it SHALL NOT require a preset file on disk for that case

### Requirement: Program metadata queries
`goggles_fc_program_get_source_info(...)` MUST populate `goggles_fc_program_source_info_t` with provenance, source name, source path, and pass count. `goggles_fc_program_get_report(...)` MUST populate `goggles_fc_program_report_t` with shader, pass, and texture counts.

#### Scenario: Consumer inspects created program metadata
- **GIVEN** a successfully created program handle
- **WHEN** the consumer queries source info and report data
- **THEN** the ABI SHALL expose source provenance/name/path and aggregate shader-pass-texture counts through the corresponding report structs

### Requirement: Chain creation and lifecycle are separate from program lifecycle
The ABI MUST expose `goggles_fc_chain_create(...)` and `goggles_fc_chain_destroy(...)` as the executable-chain lifecycle. Chain creation MUST consume an already-created device handle, an already-created program handle, and `goggles_fc_chain_create_info_t` containing target format, frames in flight, initial stage mask, and initial prechain resolution.

#### Scenario: Consumer builds an executable chain from a program
- **GIVEN** a device handle and a program handle
- **WHEN** the consumer creates a chain
- **THEN** the chain lifecycle SHALL be a distinct step from program creation
- **AND** creation inputs SHALL include stage-mask and prechain-resolution configuration

### Requirement: Chain runtime operations
The chain runtime surface MUST expose `goggles_fc_chain_bind_program(...)`, `goggles_fc_chain_clear(...)`, `goggles_fc_chain_resize(...)`, `goggles_fc_chain_set_prechain_resolution(...)`, `goggles_fc_chain_get_prechain_resolution(...)`, `goggles_fc_chain_set_stage_mask(...)`, `goggles_fc_chain_retarget(...)`, and `goggles_fc_chain_record_vk(...)`. Stage enablement MUST be controlled through stage masks, not a stage-policy API.

#### Scenario: Consumer reconfigures an existing chain
- **GIVEN** a live chain handle
- **WHEN** the consumer changes source extent, prechain resolution, stage mask, target format, or bound program
- **THEN** it SHALL do so through the dedicated chain reconfiguration calls
- **AND** the public ABI SHALL use `goggles_fc_chain_set_stage_mask(...)` rather than legacy stage-policy naming

### Requirement: Vulkan record call shape
`goggles_fc_chain_record_vk(...)` MUST consume `goggles_fc_record_info_vk_t`, including command buffer, source image, source view, source extent, target view, target extent, frame index, scale mode, and integer scale. The public scale-mode contract MUST use the `GOGGLES_FC_SCALE_MODE_*` constants.

#### Scenario: Host records one frame
- **GIVEN** a live chain and a Vulkan command buffer already chosen by the host
- **WHEN** the host records filter-chain work for a frame
- **THEN** the call SHALL pass recording inputs through `goggles_fc_record_info_vk_t`
- **AND** scale behavior SHALL be expressed through the public scale-mode constants

### Requirement: Chain diagnostics and report queries
The ABI MUST expose passive chain metadata queries through `goggles_fc_chain_get_report(...)`, `goggles_fc_chain_get_last_error(...)`, and `goggles_fc_chain_get_diagnostic_summary(...)`. The corresponding structs MUST expose pass-count/frame-count/stage-mask report data, status-vk-result-subsystem error data, and aggregate diagnostic event counts plus current frame.

#### Scenario: Consumer inspects recent chain state
- **GIVEN** a live chain handle
- **WHEN** the consumer requests report, last-error, or diagnostic-summary data
- **THEN** the ABI SHALL provide those results through dedicated out-struct queries
- **AND** the public diagnostics surface SHALL be passive metadata retrieval rather than a diagnostics-session lifecycle API

### Requirement: Control enumeration and mutation use count/index/info APIs
The ABI MUST expose controls through `goggles_fc_chain_get_control_count(...)`, `goggles_fc_chain_get_control_info(...)`, `goggles_fc_chain_find_control_index(...)`, `goggles_fc_chain_set_control_value_f32(...)`, `goggles_fc_chain_set_control_value_f32_by_name(...)`, `goggles_fc_chain_reset_control_value(...)`, and `goggles_fc_chain_reset_all_controls(...)`. `goggles_fc_control_info_t` MUST describe control index, stage, UTF-8 name/description, and current/default/min/max/step values.

#### Scenario: Consumer discovers and edits a control
- **GIVEN** a chain with exposed controls
- **WHEN** the consumer enumerates controls, looks one up by stage and name, or mutates/resets values
- **THEN** the public contract SHALL use count/index/info and direct mutation/reset calls
- **AND** it SHALL NOT require snapshot ownership objects for control listing

### Requirement: Installed standalone consumer contract
The standalone package MUST provide a self-sufficient installed C ABI for external consumers. External consumers MUST be able to compile and link against the installed package using `goggles/filter_chain.h`, the exported targets, and required third-party headers. The installed package SHALL NOT provide `goggles_filter_chain.h` as a compatibility shim.

#### Scenario: External C consumer uses the installed package
- **GIVEN** an external consumer resolves the installed standalone filter-chain package
- **WHEN** it builds against the public C ABI
- **THEN** the installed package SHALL provide the required public declarations and linkable ABI surface without Goggles-private source-tree headers
