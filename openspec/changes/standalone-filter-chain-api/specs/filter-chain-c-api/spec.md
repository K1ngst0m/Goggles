# Delta for filter-chain-c-api

## MODIFIED Requirements

### Requirement: Public Header and Export Surface

The standalone filter-chain C API MUST be defined by a library-owned installed public surface and
MUST use the `goggles_fc_` prefix for public types, constants, and functions. The public surface
MUST represent the standalone library contract rather than Goggles integration details, and it MUST
NOT require deprecated aliases or compatibility entry points for prior `goggles_chain_*` or `fc_*`
names in v1.

The final shipped public C surface MUST contain only the clean-slate contract. Installed headers,
exported symbols, package metadata, public examples, and public-facing usage notes MUST NOT retain
legacy names, deprecated annotations for removed APIs, or side-by-side documentation for both the
old and new contracts.

#### Scenario: Host compiles against the standalone public header

- GIVEN an external host integrates the installed standalone package
- WHEN it includes the public C header and resolves exported symbols
- THEN every required public declaration SHALL use the `goggles_fc_` prefix
- AND the host SHALL NOT need legacy `goggles_chain_*` or `fc_*` declarations to consume v1

#### Scenario: Installed package does not ship dual public C surfaces

- GIVEN a maintainer inspects the installed standalone package and its package-facing examples/docs
- WHEN the clean-slate redesign is complete
- THEN the shipped surface SHALL expose only `goggles_fc_*` public declarations and examples
- AND it SHALL NOT ship deprecated legacy C headers, compatibility aliases, or stale usage text for removed names

### Requirement: Version and Capability Negotiation

The v1 standalone contract MUST report itself as Vulkan-only and MUST use version and capability
queries to describe optional behavior without implying non-Vulkan backend support. Capability
negotiation MUST allow a host to determine whether file-based preset loading, memory-based preset
loading, logging callbacks, and other optional extensions are available without probing symbol
presence dynamically.

The required v1 query surface MUST include explicit `goggles_fc_get_api_version(...)`,
`goggles_fc_get_abi_version(...)`, and `goggles_fc_get_capabilities(...)` families. Those queries
MUST be callable without creating instance, device, program, or chain objects, and the capability
report MUST explicitly indicate that v1 is Vulkan-only while enumerating any optional source-loading
or diagnostics extensions.

#### Scenario: Host verifies backend scope before creating objects

- GIVEN a host queries the standalone library before creating runtime objects
- WHEN it inspects the reported API version, ABI version, and capabilities
- THEN it SHALL be able to determine that v1 supports Vulkan integration
- AND it SHALL NOT infer support for non-Vulkan backends unless a future capability explicitly adds them

### Requirement: Runtime Lifecycle and State Semantics

The standalone C API MUST expose a first-class object model centered on `instance`, `device`,
`program`, and `chain` responsibilities. Lifecycle rules MUST make ownership, dependency ordering,
and valid call sequencing explicit for FFI consumers, and each object family MUST expose creation
and destruction semantics that are independent of Goggles runtime types.

The following matrix defines the normative minimum v1 API families and their lifecycle intent:

| Function family | Owning object | v1 disposition |
|---|---|---|
| `goggles_fc_get_api_version`, `goggles_fc_get_abi_version`, `goggles_fc_get_capabilities` | library | reshaped from implicit/partial version reporting into explicit process-global queries |
| `goggles_fc_instance_create`, `goggles_fc_instance_destroy` | `instance` | reshaped from implicit global/session state into explicit instance ownership |
| `goggles_fc_instance_set_log_callback` | `instance` | renamed and reshaped from shared/global logger wiring into per-instance callback registration |
| `goggles_fc_device_create_vk`, `goggles_fc_device_destroy` | `device` | reshaped from monolithic runtime creation into explicit Vulkan device binding |
| `goggles_fc_program_create`, `goggles_fc_program_destroy` | `program` | reshaped from chain-local preset loading into immutable program creation |
| `goggles_fc_program_get_source_info`, `goggles_fc_program_get_report` | `program` | new first-class program provenance and diagnostics queries |
| `goggles_fc_chain_create`, `goggles_fc_chain_destroy` | `chain` | reshaped from monolithic runtime lifetime into executable chain lifetime |
| `goggles_fc_chain_bind_program`, `goggles_fc_chain_clear` | `chain` | bind is new explicit program attachment; clear preserves runtime reset intent but moves under chain ownership |
| `goggles_fc_chain_record_vk` | `chain` | preserved in purpose, renamed to `goggles_fc_` and narrowed to Vulkan-only v1; accepts `scale_mode` as a `GOGGLES_FC_SCALE_MODE_*` constant |
| `goggles_fc_chain_set_prechain_resolution` | `chain` | dedicated runtime update path for prechain resolution after chain creation |
| `goggles_fc_chain_resize`, `goggles_fc_chain_retarget` | `chain` | preserved in behavior, renamed and split into explicit resize/retarget families |
| `goggles_fc_chain_get_control_count`, `goggles_fc_chain_get_control_info`, `goggles_fc_chain_find_control_index`, `goggles_fc_chain_set_control_*` | `chain` | preserved in purpose, reshaped into explicit metadata/query and index-based or semantic-name-based set families so callers do not depend on transient control ordering |
| `goggles_fc_chain_get_report`, `goggles_fc_chain_get_last_error` | `chain` | reshaped from ad hoc diagnostics into explicit chain-scoped report/error queries |
| `goggles_fc_status_string`, `goggles_fc_is_success`, `goggles_fc_is_error` | library | new error/status helper families for FFI-friendly status inspection |

No v1 requirement SHALL depend on deprecated `goggles_chain_*` or `fc_*` entry points, and any
legacy monolithic runtime family SHALL be treated as removed rather than carried forward as a public
compatibility layer.

Control lookup and mutation MUST remain correct across runtime rebuilds, program rebinds, and other
successful replacements that can change control enumeration order. The standalone API therefore MUST
provide a semantic lookup path keyed by control stage plus control name, and hosts SHOULD prefer that
semantic path over caching enumeration indexes across reload boundaries.

The v1 C API scale mode constants SHALL expose the full standalone record-time surface:
`GOGGLES_FC_SCALE_MODE_STRETCH=0`, `GOGGLES_FC_SCALE_MODE_FIT=1`,
`GOGGLES_FC_SCALE_MODE_INTEGER=2`, `GOGGLES_FC_SCALE_MODE_FILL=3`, and
`GOGGLES_FC_SCALE_MODE_DYNAMIC=4`. Runtime translation from C API values to internal values SHALL
use explicit mapping rather than raw casts so the standalone constants remain authoritative even when
internal enum ordering differs.

#### Scenario: Host creates the standalone object graph explicitly

- GIVEN a host wants to prepare a filter runtime through the public C API
- WHEN it follows the documented lifecycle
- THEN it SHALL create and manage standalone object families with explicit boundaries for instance,
  device, program, and chain roles
- AND it SHALL NOT need Goggles-private objects or implicit global state to do so

#### Scenario: Host selects fill or dynamic scaling through the public C contract

- GIVEN a host records work through `goggles_fc_chain_record_vk(...)`
- WHEN it sets `record_info.scale_mode` to `GOGGLES_FC_SCALE_MODE_FILL` or
  `GOGGLES_FC_SCALE_MODE_DYNAMIC`
- THEN the call SHALL be valid within the standalone public contract
- AND the runtime SHALL apply the requested public scale mode without requiring Goggles-private enum translation at the callsite

#### Scenario: Host updates prechain resolution without rebuilding create info

- GIVEN a live standalone chain already exists
- WHEN the host calls `goggles_fc_chain_set_prechain_resolution(...)` with a positive extent
- THEN the runtime SHALL update the chain's prechain resolution through that dedicated API
- AND the host SHALL NOT need to recreate the chain solely to change prechain resolution

### Requirement: Vulkan Create Input Validation

Vulkan-facing creation APIs MUST accept host-provided Vulkan handles and queue selection as borrowed
inputs at the host/library boundary, while the library MUST own internal compilation state, caches,
pipeline objects, setup/upload command helpers, and intermediate runtime resources derived from that
context. The public creation contract MUST make borrowed-versus-owned Vulkan resources explicit.

#### Scenario: Host provides device and queue handles

- GIVEN a host has already selected Vulkan instance, physical device, logical device, and queue ownership
- WHEN it creates standalone filter-chain device or chain objects
- THEN the host SHALL provide the required Vulkan handles through the public creation contract
- AND the library SHALL retain ownership only of its own derived runtime resources rather than host submission objects

### Requirement: Error Model and Diagnostics Contract

All fallible APIs MUST return standalone status codes and MUST NOT require exceptions. Logging and
diagnostic delivery MUST be host-routable through callback or sink registration contracts that are
scoped to standalone objects or sessions, and the library MUST NOT require shared/global logger
symbols as part of the public contract.

The public contract MUST distinguish unstructured log delivery from structured diagnostic/report
queries. Logging callbacks MUST carry human-readable severity/domain/message data for immediate host
consumption, while report-query families MUST expose structured machine-readable status for program
and chain inspection without requiring the host to parse free-form log text.

#### Scenario: Host routes diagnostics into its own logging system

- GIVEN a host embeds the standalone library inside an application with its own logging stack
- WHEN it registers a logging callback or sink through the public API
- THEN standalone diagnostics SHALL be delivered through that host-provided route
- AND linking the library SHALL NOT require exporting or sharing global logger state between host and library

### Requirement: UTF-8 Path and Length-Based API Contract

The standalone C API MUST support preset loading from both file and memory sources. File-based
entry points MUST continue to define UTF-8 and explicit-length behavior for path input, and memory-
based entry points MUST accept caller-provided preset bytes without requiring a filesystem path or a
public shader directory contract.

All public structs that expose textual identifiers, paths, diagnostics, or debug names MUST use the
same canonical representation: a UTF-8 byte pointer plus an explicit byte length, with NUL
termination optional and not semantically required. When a public struct accepts a UTF-8 path, the
path bytes MUST represent the host's intended filesystem path exactly as supplied, and the library
MUST treat the byte-length field rather than sentinel termination as authoritative.

Preset provenance and relative resolution MUST follow these rules:

- File-backed preset sources MUST record file provenance and derive the default base path from the
  parent directory of the provided preset path.
- Memory-backed preset sources MUST record memory provenance and MAY additionally supply a UTF-8
  `source_name` for diagnostics plus either a UTF-8 `base_path` or import/resolver callbacks.
- Relative preset includes and external texture references from a file-backed source MUST resolve
  relative to that source's derived base path.
- Relative preset includes and external texture references from a memory-backed source MUST resolve
  through the host-provided resolver when one is registered; otherwise they MUST resolve relative to
  the explicit memory-source `base_path` when present.
- If a memory-backed source references relative external content and neither a resolver nor a
  `base_path` is provided, program creation MUST fail with a deterministic validation status rather
  than probing process working-directory state.

### Requirement: Callback and Returned-String Lifetime Contract

The standalone C API MUST define explicit lifetime and ownership rules for all callback-delivered
data and for strings or buffers returned by API functions, so that FFI consumers in any language can
manage memory correctly without relying on C-specific conventions.

#### Callback-delivered data

- Pointers passed into a host-provided callback (such as `goggles_fc_log_callback_t`) are borrowed
  by the host for the duration of that callback invocation only. The library owns the backing memory,
  and the host MUST NOT retain, free, or write through those pointers after the callback returns.
  If the host needs the data beyond the callback scope it MUST copy the content before returning.

- For import callbacks (`goggles_fc_import_read_fn_t`), the host (callee) allocates and returns a
  buffer through `out_bytes` / `out_byte_count`. The library borrows that buffer and MUST release it
  by calling the paired `goggles_fc_import_free_fn_t` with the same pointer and size once it has
  finished reading. The host MUST NOT free the buffer itself; it MUST wait for the library's
  `free_fn` call.

#### Strings returned by query functions

- The string returned by `goggles_fc_status_string(...)` is a pointer to a library-owned static
  string literal. It remains valid for the lifetime of the process and MUST NOT be freed by the
  caller.

- UTF-8 views written into caller-provided output structs by query functions (for example the `name`
  and `description` fields in `goggles_fc_control_info_t`, or the `source_name` and `source_path`
  fields in `goggles_fc_program_source_info_t`) point into library-owned internal storage. These
  pointers remain valid only as long as the owning object (program, chain, etc.) is alive and has not
  undergone a state change that invalidates the queried data (such as `goggles_fc_chain_bind_program`
  replacing the bound program). Callers that need the strings beyond that scope MUST copy them.

#### Thread safety of callbacks

- The library MAY invoke a logging callback from any thread that performs library work. The host
  callback implementation MUST be safe to call concurrently from multiple threads if the host
  performs multi-threaded recording or object creation on the same instance.

- Import callbacks (`read_fn`, `free_fn`) are invoked synchronously on the calling thread during
  `goggles_fc_program_create`. The library SHALL NOT invoke import callbacks from background threads.

#### Scenario: Host copies log message in callback for deferred processing

- GIVEN a host registers a `goggles_fc_log_callback_t` with the standalone instance
- WHEN the library invokes the callback with a `goggles_fc_log_message_t` pointer
- THEN the host SHALL treat the message and its embedded UTF-8 views as borrowed for the callback
  duration only
- AND the host SHALL copy any data it needs to retain before returning from the callback

#### Scenario: Library frees import buffer through paired free callback

- GIVEN a host provides `goggles_fc_import_read_fn_t` and `goggles_fc_import_free_fn_t` callbacks
- WHEN the library calls `read_fn` and receives a host-allocated buffer
- THEN the library SHALL release that buffer by calling `free_fn` with the original pointer and size
- AND the host SHALL NOT free the buffer independently

#### Scenario: Host loads a preset from memory only

- GIVEN a host has preset content in memory and no preset file on disk
- WHEN it calls the memory-based preset loading entry point
- THEN the standalone library SHALL initialize the preset from the provided byte buffer
- AND the host SHALL NOT need to materialize a temporary file or configure a public shader directory

#### Scenario: Memory source omits resolver and base path for relative imports

- GIVEN a host provides a memory-backed preset source whose content references a relative include or texture
- WHEN it does not provide import callbacks and does not provide an explicit memory-source base path
- THEN program creation SHALL fail with a deterministic validation status
- AND the runtime SHALL NOT attempt fallback resolution through the process working directory or Goggles-private paths

### Requirement: Ownership, Handle Provenance, and Pointer Retention

The public contract MUST make host/library ownership boundaries explicit. The library MUST NOT
retain pointers to caller-owned transient input buffers beyond the documented call lifetime, and it
MUST document which handles remain borrowed from the host versus which resources become library-owned
after object creation.

#### Scenario: Host frees transient preset input after load request

- GIVEN a host passes file-path bytes or preset-memory bytes into a load call
- WHEN the call returns successfully or with validation failure
- THEN the host SHALL be free to release or mutate its transient input buffers according to the documented call lifetime
- AND the library SHALL preserve memory safety without depending on continued caller ownership of those transient buffers

### Requirement: Installed C ABI Consumer Contract

The installed standalone C ABI MUST be consumable without Goggles-private headers, repository-
relative assets, or Goggles-defined public abstractions. Built-in internal shaders and related
library-owned assets required for normal runtime behavior MUST be packaged as library-owned runtime
content rather than exposed as a required public `shader_dir` input.

#### Scenario: Installed consumer runs without configuring a shader directory

- GIVEN an external C consumer links the installed standalone package
- WHEN it creates the runtime and loads a supported preset
- THEN the runtime SHALL resolve required built-in internal shader or asset content through library-owned packaging
- AND the consumer SHALL NOT need to supply a public `shader_dir` path merely to enable normal operation

#### Scenario: Installed package ships no stale runtime assumptions

- GIVEN a maintainer reviews the installed public contract and package metadata
- WHEN it verifies the final v1 standalone package
- THEN no exported config, documented environment, or public example SHALL require removed install-only runtime inputs such as `shader_dir`
- AND built-in runtime behavior SHALL be described entirely in terms of library-owned assets plus the documented file/memory source contracts

## REMOVED Requirements

### Requirement: Compatibility and Evolution Policy for v1.x

(Reason: This change is a clean-slate standalone redesign where backward compatibility with prior
`goggles_chain_*` naming and handle families is explicitly out of scope for v1 planning.)
