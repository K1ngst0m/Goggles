# filter-chain-cpp-wrapper Specification

## Purpose
Defines the current public C++ wrapper around the standalone filter-chain C ABI: the canonical installed C++20 entrypoint, RAII ownership, result-based error propagation, namespace boundaries, and the exact wrapper methods currently exposed to C++ consumers.

## Requirements
### Requirement: Public wrapper surface and namespace contract
The standalone package MUST provide its public C++ wrapper surface through `goggles/filter_chain.hpp` as the canonical installed C++20 entrypoint. Supporting public types remain split by ownership: wrapper enums and `Extent2D` in `goggles::filter_chain`, plus filter-control and Vulkan-context support types in `goggles::fc`.

#### Scenario: Consumer includes explicit installed wrapper headers
- **GIVEN** a C++ consumer integrates the standalone filter-chain package
- **WHEN** it includes `goggles/filter_chain.hpp`
- **THEN** the RAII runtime API SHALL be available from `goggles::filter_chain`
- **AND** the installed package SHALL NOT require or provide `goggles_filter_chain.h` as a compatibility shim
- **AND** support headers SHALL retain their current namespace ownership split instead of collapsing everything into one namespace

### Requirement: RAII ownership for instance, device, program, and chain
The wrapper MUST provide move-only RAII classes `Instance`, `Device`, `Program`, and `Chain`. Each wrapper class MUST own exactly one corresponding `goggles_fc_*` handle, destroy that handle in its destructor, expose `handle()` for interop, and expose `explicit operator bool()` for validity checks.

#### Scenario: Wrapper-owned object leaves scope
- **GIVEN** a wrapper object owns a live underlying C handle
- **WHEN** that wrapper object is destroyed or overwritten by move assignment
- **THEN** the corresponding C ABI destroy function SHALL run exactly once
- **AND** normal C++ callsites SHALL NOT manage raw destroy calls directly

### Requirement: Result-based error propagation
All fallible wrapper operations MUST return `goggles::Result<T>` or `goggles::Result<void>`. The installed package MUST define that result/error surface in `goggles/filter_chain/error.hpp`, and `goggles/filter_chain/result.hpp` MAY remain as a convenience forwarding include to the same definitions. Expected runtime failures MUST NOT require exceptions as part of the public contract.

#### Scenario: Wrapper call fails
- **GIVEN** a wrapper operation encounters an expected runtime failure from the C ABI
- **WHEN** the C++ consumer receives the result
- **THEN** the failure SHALL be represented as a failed `goggles::Result`
- **AND** the wrapper contract SHALL NOT require exception handling for that path

### Requirement: Wrapper call signatures mirror C ABI structs
The current wrapper API MUST accept the public C ABI structs and callback types directly rather than re-expressing them as `vk::`-typed or bespoke C++ configuration objects. Wrapper creation, recording, retargeting, and metadata queries MUST use `goggles_fc_*` structs where applicable.

#### Scenario: Consumer configures wrapper calls
- **GIVEN** a C++ consumer prepares instance, Vulkan device, preset source, chain create, retarget, or record inputs
- **WHEN** it calls the wrapper
- **THEN** it SHALL pass the corresponding `goggles_fc_*` public structs
- **AND** the current wrapper contract SHALL NOT promise a `vk::`-typed wrapper-only configuration surface

### Requirement: Instance and device wrapper operations
`Instance` MUST expose `create(const goggles_fc_instance_create_info_t*)` and `set_log_callback(goggles_fc_log_callback_t, void*)`. `Device` MUST expose `create(Instance&, const goggles_fc_vk_device_create_info_t*)`.

#### Scenario: Consumer bootstraps wrapper runtime state
- **GIVEN** a C++ consumer wants to initialize the standalone runtime
- **WHEN** it creates an instance, optionally installs a log callback, and binds a Vulkan device
- **THEN** the wrapper SHALL provide those operations through `Instance` and `Device`

### Requirement: Program wrapper operations use preset-source creation
`Program` MUST expose `create(Device&, const goggles_fc_preset_source_t*)`, `get_source_info()`, and `get_report()`. The wrapper contract MUST use program creation from a preset-source descriptor rather than legacy `preset_load` naming.

#### Scenario: Consumer creates a wrapper program from preset source
- **GIVEN** a device wrapper and a preset source descriptor
- **WHEN** the consumer creates a program and inspects its metadata
- **THEN** the public wrapper SHALL create through `Program::create(...)`
- **AND** source/report inspection SHALL be available through dedicated query methods

### Requirement: Chain wrapper operations reflect current public methods
`Chain` MUST expose `create(Device&, const Program&, const goggles_fc_chain_create_info_t*)`, `bind_program(...)`, `clear()`, `resize(...)`, `set_prechain_resolution(...)`, `get_prechain_resolution()`, `set_stage_mask(...)`, `retarget(...)`, `record_vk(...)`, `get_diagnostic_summary()`, `get_report()`, `get_last_error()`, `get_control_count()`, `find_control_index(...)`, `get_control_info(...)`, both `set_control_value_f32(...)` overloads, `reset_control_value(...)`, and `reset_all_controls()`.

#### Scenario: Consumer drives a chain through the wrapper
- **GIVEN** a live chain wrapper
- **WHEN** the consumer rebonds programs, clears state, resizes, queries prechain resolution, retargets, records, inspects reports/errors, or queries, mutates, and resets controls
- **THEN** the wrapper SHALL expose those operations as `Chain` methods
- **AND** stage enablement SHALL be expressed as `set_stage_mask(...)`

### Requirement: Wrapper parity is intentionally partial
The current C++ wrapper MUST match the methods it actually publishes and SHALL NOT be treated as a full 1:1 mirror of every C ABI entrypoint. The retained runtime-policy and reset helpers SHALL be wrapped directly in `Chain`; consumers that need other unsupported ABI operations MAY call the C ABI directly.

#### Scenario: Consumer needs an ABI feature not wrapped today
- **GIVEN** a C++ consumer needs a public C ABI operation that has no dedicated wrapper method
- **WHEN** it inspects the current wrapper contract
- **THEN** the absence of that wrapper method SHALL be considered current supported behavior
- **AND** direct C ABI interop through `handle()` SHALL remain the compatibility path

### Requirement: Global helper forwarding
The wrapper MUST expose inline free functions `get_api_version()`, `get_abi_version()`, `get_capabilities()`, and `status_string(...)` that forward the corresponding global C ABI queries.

#### Scenario: Consumer queries global runtime metadata in C++
- **GIVEN** a C++ consumer needs version, capability, or status-string data without creating wrapper objects first
- **WHEN** it uses the public wrapper helpers
- **THEN** the wrapper SHALL forward those queries directly to the C ABI global helpers

### Requirement: Installed wrapper consumer contract
The installed standalone package MUST provide a self-sufficient C++ wrapper surface for external consumers using only installed public headers, exported targets, and required third-party dependencies. External C++ consumers MUST NOT require Goggles-private source-tree headers to use the wrapper, and the installed surface SHALL be satisfied by `goggles/filter_chain.hpp`, `goggles/filter_chain.h`, and any required support headers under `goggles/filter_chain/`.

#### Scenario: External C++ consumer uses installed wrapper
- **GIVEN** an external C++ consumer resolves the installed standalone package
- **WHEN** it includes the wrapper headers and builds against the exported library targets
- **THEN** the installed package SHALL provide the required public wrapper declarations without Goggles-private headers
