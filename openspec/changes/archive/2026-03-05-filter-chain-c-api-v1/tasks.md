## 1. Public C ABI Surface and Build Wiring

- [x] 1.1 Add `include/goggles_filter_chain.h` with v1 macros, opaque handles, fixed-width scalar typedefs/constants, struct definitions, and inline init helpers.
- [x] 1.2 Wire header install/export packaging so v1 symbols are visible for shared/static builds on supported toolchains.
- [x] 1.3 Implement global metadata APIs (`goggles_chain_api_version`, `goggles_chain_abi_version`, `goggles_chain_capabilities_get`, `goggles_chain_status_to_string`) with ABI-locked values.

## 2. Runtime Lifecycle and Preset/Resize APIs

- [x] 2.1 Implement C runtime handle wrapper and lifecycle state machine (`CREATED`, `READY`, `DEAD`) with null-safe idempotent destroy semantics.
- [x] 2.2 Implement `goggles_chain_create_vk` and `goggles_chain_create_vk_ex` with required input validation, `struct_size` enforcement, and out-parameter failure guarantees.
- [x] 2.3 Implement `goggles_chain_preset_load` and `goggles_chain_preset_load_ex` with UTF-8/length validation and state-preserving failure behavior.
- [x] 2.4 Implement `goggles_chain_handle_resize` and prechain resolution get/set APIs with extent validation in `CREATED` and `READY` states.

## 3. Record API and Stage Policy

- [x] 3.1 Implement `goggles_chain_record_vk` validation for command buffer, source/target inputs, scale/integer rules, and `frame_index < num_sync_indices`.
- [x] 3.2 Preserve execution order and stage semantics (`prechain -> effect -> postchain`) while keeping record path command-record-only.
- [x] 3.3 Implement `goggles_chain_stage_policy_set/get` with strict stage-mask validation (reject zero and unknown bits).

## 4. Control Snapshot and Mutation APIs

- [x] 4.1 Implement snapshot ownership APIs (`control_list`, `control_list_stage`, `snapshot_get_count`, `snapshot_get_data`, `snapshot_destroy`) with deterministic ordering and valid empty postchain snapshot behavior.
- [x] 4.2 Implement control mutation APIs (`control_set_value`, `control_reset_value`, `control_reset_all`) using stable `control_id` keys and finite-value clamping rules.
- [x] 4.3 Ensure non-finite control values (`NaN`/`Inf`) are rejected with `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT` and do not mutate runtime control state.

## 5. Diagnostics, Ownership, and Integration

- [x] 5.1 Implement per-runtime last-error diagnostics (`goggles_chain_error_last_info_get`) with capability-gated `NOT_SUPPORTED` behavior.
- [x] 5.2 Ensure no retention of caller-provided path/struct pointers after call return and enforce handle provenance checks where detectable.
- [x] 5.3 Update internal render/backend integration to use the C shim while preserving existing runtime behavior and synchronization ownership.

## 6. Contract and Conformance Testing

- [x] 6.1 Add unit tests for lifecycle matrix, invalid call-state behavior, null-safe/idempotent destroy, and out-parameter-on-failure rules.
- [x] 6.2 Add tests for `struct_size` matrix, enum/scalar/path/extents validation, UTF-8 `_ex` path rules, and non-finite control rejection.
- [x] 6.3 Add control snapshot tests for ordering, lifetime/borrowed-pointer validity, and empty postchain snapshot contract.
- [x] 6.4 Add record-path tests for frame-index bounds and invalid-argument no-command-recorded behavior; add integration coverage for create->load->record->destroy flow.
- [x] 6.5 Add ABI durability checks for public struct layout and scalar widths, plus shared/static cross-compiler smoke coverage in local test lanes.

## 7. Verification and Spec Alignment

- [x] 7.1 Run `pixi run build -p asan` and fix all build/lint/compile issues introduced by the change.
- [x] 7.2 Run `pixi run test -p asan` and `ctest --preset test -R "goggles_unit_tests" --output-on-failure` for contract and integration validation.
- [x] 7.3 Run `pixi run build -p quality` and resolve quality-gate findings.
