# Delta for goggles-filter-chain

## MODIFIED Requirements

### Requirement: Namespace Identity for FC-Owned Types

(Previously: FC-owned public types resided in `goggles::render` namespace alongside host render code, creating ambiguity about ownership boundaries.)

All filter-chain-owned C++ types, functions, and constants SHALL use the `goggles::fc` namespace. The `goggles::render` namespace SHALL NOT contain any FC-owned type definitions after migration. The canonical C ABI entrypoint (`goggles/filter_chain.h`) SHALL NOT be affected by this namespace change, as C headers do not use C++ namespaces.

#### Scenario: Public header namespace migration

- **GIVEN** the public headers `vulkan_context.hpp` and `filter_controls.hpp` under `filter-chain/include/goggles/filter_chain/`
- **WHEN** namespace declarations are inspected
- **THEN** all type definitions SHALL use `namespace goggles::fc`
- **AND** no `namespace goggles::render` declarations SHALL remain in those headers

#### Scenario: Internal source namespace migration

- **GIVEN** all implementation files under `filter-chain/src/` (~48 files)
- **WHEN** namespace declarations and qualified name references are inspected
- **THEN** all FC-owned type references SHALL use `goggles::fc` namespace
- **AND** zero occurrences of `goggles::render` SHALL remain in FC-owned source files

#### Scenario: Contract test namespace migration

- **GIVEN** contract test files under `filter-chain/tests/contract/` (~11 files)
- **WHEN** namespace references are inspected
- **THEN** all FC type references SHALL use `goggles::fc` namespace
- **AND** zero occurrences of `goggles::render` for FC-owned types SHALL remain

#### Scenario: Host code namespace update

- **GIVEN** host files that reference FC-owned public types (`VulkanContext`, `FilterControlDescriptor`, `FilterControlId`, `FilterControlStage`, `make_filter_control_id()`, `clamp_filter_control_value()`)
- **WHEN** qualified name references in host backend and test files are inspected
- **THEN** all references SHALL use `goggles::fc::` qualification
- **AND** no host file SHALL reference these types via `goggles::render::`

#### Scenario: C ABI unaffected by namespace change

- **GIVEN** the canonical C ABI header `goggles/filter_chain.h`
- **WHEN** the header contents are inspected after namespace migration
- **THEN** the header SHALL be unchanged
- **AND** all C ABI function signatures and type definitions SHALL remain identical

#### Scenario: Compilation succeeds after namespace migration

- **GIVEN** the namespace rename has been applied across all FC and host files
- **WHEN** the full monorepo CI runs (`pixi run ci --runner container --cache-mode warm --lane all`)
- **THEN** compilation SHALL succeed with zero errors
- **AND** all tests SHALL pass

#### Scenario: No residual goggles::render in FC code

- **GIVEN** the completed namespace migration
- **WHEN** a grep for `goggles::render` is executed across all files under `filter-chain/`
- **THEN** zero matches SHALL be found
- **AND** the only `goggles::render` references in the repository SHALL be in host-owned code for host-owned types

### Requirement: Semgrep fixture namespace update

Semgrep test fixtures that reference FC-owned types SHALL use the `goggles::fc` namespace to match the migrated codebase.

#### Scenario: Semgrep fixtures use updated namespace

- **GIVEN** semgrep fixture files under `tests/semgrep/fixtures/src/render/chain/`
- **WHEN** fixture source files referencing FC-owned types are inspected
- **THEN** those references SHALL use `goggles::fc` namespace
- **AND** fixtures SHALL remain valid inputs for their associated semgrep rules

## ADDED Requirements

### Requirement: Host Error Type Independence

Host backend files SHALL NOT include any filter-chain headers for error/result types. Host code that requires `Result<T>` or equivalent error types SHALL use its own `util/error.hpp` (or equivalent host-owned header) instead of `<goggles/filter_chain/result.hpp>`.

#### Scenario: Host backend headers use own error types

- **GIVEN** the host backend headers `vulkan_error.hpp`, `vulkan_debug.hpp`, `render_output.hpp`, and `external_frame_importer.hpp`
- **WHEN** include directives are inspected
- **THEN** none of these files SHALL include `<goggles/filter_chain/result.hpp>`
- **AND** each SHALL use a host-owned error type header for `Result<T>` definitions

#### Scenario: Host builds without FC source tree for error types

- **GIVEN** the host codebase with FC consumed as an installed package (headers only)
- **WHEN** host backend files are compiled
- **THEN** compilation SHALL succeed without FC private headers being available
- **AND** no error type resolution SHALL depend on FC internal header paths

#### Scenario: ODR safety with duplicated error types

- **GIVEN** both host and FC define compatible error/result types independently
- **WHEN** both are linked in the same binary
- **THEN** ODR guards (e.g., `GOGGLES_ERROR_TYPES_DEFINED`) SHALL prevent duplicate definitions
- **AND** the linked binary SHALL exhibit no ODR violations

### Requirement: Visual Test Public API Boundary

Goggles host-side visual and integration tests SHALL NOT include FC private/internal headers. All Goggles-side interactions with the filter-chain runtime SHALL use only the durable, caller-facing public API surface. Goggles host tests SHALL NOT require the stable public boundary to preserve intermediate pass capture or public diagnostics-session lifecycle affordances.

#### Scenario: Visual test uses public API only

- **GIVEN** the visual test file `tests/visual/runtime_capture.cpp`
- **WHEN** include directives are inspected
- **THEN** no includes of FC internal headers (e.g., `chain_runtime.hpp`, `diagnostic_policy.hpp`) SHALL be present
- **AND** all FC interactions SHALL use durable public API functions for lifecycle, record, controls, reports, or passive metadata queries only

#### Scenario: Visual test compiles with installed FC headers only

- **GIVEN** the filter-chain is consumed as an installed package
- **WHEN** `tests/visual/runtime_capture.cpp` is compiled
- **THEN** compilation SHALL succeed using only installed public FC headers
- **AND** no FC `src/` internal header paths SHALL be required

#### Scenario: Visual test passes after refactor

- **GIVEN** the visual test has been refactored to use the public API
- **WHEN** the full test suite is executed
- **THEN** `tests/visual/runtime_capture.cpp` SHALL compile and pass
- **AND** any host-visible diagnostics retained for Goggles SHALL be available through passive public metadata/report queries rather than public diagnostics-session lifecycle control

#### Scenario: Goggles tests do not rely on stable pass capture

- **GIVEN** Goggles host-side tests after boundary convergence
- **WHEN** their FC-facing assertions are inspected
- **THEN** they SHALL validate final host-observable behavior through durable public runtime APIs
- **AND** they SHALL NOT require stable caller-facing intermediate pass capture APIs

### Requirement: Host Integration Test Boundary Enforcement

Goggles host integration tests that exercise the FC boundary SHALL use only the durable public API surface and SHALL focus on end-to-end host integration behavior. No host test file SHALL include FC private headers from `filter-chain/src/`.

#### Scenario: Host boundary tests use public headers only

- **GIVEN** host integration test files (`test_filter_boundary_contracts.cpp`, `test_vulkan_backend_subsystem_contracts.cpp`, `test_filter_chain_retarget.cpp`)
- **WHEN** include directives are inspected
- **THEN** no includes referencing paths under `filter-chain/src/` SHALL be present
- **AND** all FC interactions SHALL use the public installed header surface

#### Scenario: Host tests compile with FC as installed dependency

- **GIVEN** the filter-chain is consumed via `add_subdirectory()` or `find_package()`
- **WHEN** host integration tests are compiled
- **THEN** compilation SHALL succeed using only the target's public include interface
- **AND** no `-I` flags pointing into FC `src/` directories SHALL be required

#### Scenario: Goggles retains host-only verification scope

- **GIVEN** the verification split after extraction
- **WHEN** Goggles-owned FC-facing tests are reviewed
- **THEN** they SHALL cover host wiring, integration, and end-to-end runtime behavior
- **AND** standalone `filter-chain` SHALL own intermediate-pass golden and other diagnostics-heavy verification that depends on internal capture-oriented capabilities

### Requirement: Stable Caller-Facing Diagnostics Boundary

The stable caller-facing diagnostics surface SHALL be limited to the minimum justified boundary. If a public diagnostics summary is retained, it SHALL be exposed as passive chain metadata or report state. Stable external-consumer diagnostics SHALL NOT depend on a public diagnostics-session lifecycle contract unless a later approved change explicitly justifies and re-specifies that lifecycle as durable API.

#### Scenario: Passive diagnostics summary

- **GIVEN** a caller needs host-visible diagnostic state from the runtime
- **WHEN** the caller queries the stable diagnostics surface
- **THEN** any returned summary SHALL be available as passive metadata or report state
- **AND** retrieving that summary SHALL NOT require the caller to create, manage, or destroy a public diagnostics session

#### Scenario: Minimal stable diagnostics policy

- **GIVEN** a caller configures stable public diagnostics behavior
- **WHEN** the stable public policy surface is inspected
- **THEN** the caller-facing policy SHALL be limited to the minimum justified surface
- **AND** broader session-lifecycle-driven or capture-oriented policy controls SHALL NOT be treated as stable caller-facing requirements without explicit later justification

### Requirement: Stable Runtime Policy and Reset Helpers

The local-boundary convergence for this change SHALL retain only the following provisional runtime-policy/reset APIs as supported public surface: `goggles_fc_chain_set_stage_mask`, `goggles_fc_chain_set_prechain_resolution`, `goggles_fc_chain_get_prechain_resolution`, `goggles_fc_chain_reset_control_value`, and `goggles_fc_chain_reset_all_controls`. These APIs SHALL remain aligned across the installed C API, the public C++ wrapper, and contract coverage.

#### Scenario: Stable runtime policy helpers remain public

- **GIVEN** the converged stable caller-facing boundary for the extracted library
- **WHEN** the retained runtime-policy APIs are reviewed
- **THEN** `goggles_fc_chain_set_stage_mask`, `goggles_fc_chain_set_prechain_resolution`, and `goggles_fc_chain_get_prechain_resolution` SHALL remain present in the installed public API
- **AND** they SHALL be treated as durable cross-host runtime policy controls rather than migration-only diagnostics or capture seams

#### Scenario: Stable reset helpers remain public

- **GIVEN** the converged stable caller-facing boundary for the extracted library
- **WHEN** the retained reset helpers are reviewed
- **THEN** `goggles_fc_chain_reset_control_value` and `goggles_fc_chain_reset_all_controls` SHALL remain present in the installed public API
- **AND** they SHALL be treated as part of the stable caller-facing control-mutation workflow

#### Scenario: Public C and C++ surfaces stay aligned

- **GIVEN** the retained runtime-policy and reset-helper API set
- **WHEN** the installed public headers are inspected
- **THEN** the C API and `goggles::filter_chain::Chain` wrapper SHALL expose the same retained helper set where the wrapper provides coverage for the underlying chain surface
- **AND** no retained helper SHALL exist only as an undocumented provisional export

### Requirement: Intermediate Pass Capture Is Non-Stable

Intermediate pass capture SHALL be treated as internal or standalone-test infrastructure unless a later approved change explicitly justifies it as durable cross-host public API. The extraction change SHALL NOT be considered complete by merely preserving pass capture as stable caller-facing surface for Goggles-hosted tests.

#### Scenario: Stable contract excludes pass capture by default

- **GIVEN** the converged stable caller-facing boundary for the extracted library
- **WHEN** stable public requirements are reviewed
- **THEN** intermediate pass capture SHALL NOT be required as part of the stable caller-facing contract
- **AND** any remaining capture capability SHALL be treated as non-stable or internal until explicitly re-justified

### Requirement: Migration Completion Requires Transitional Cleanup

The extraction change SHALL NOT be considered complete until migration to the narrowed public boundary is finished and stale transitional code is removed, internalized, or reassigned. This includes stale public diagnostics-session or pass-capture affordances kept only for migration, plus Goggles-side tests or wrappers that preserve those affordances after their ownership has moved.

#### Scenario: Completion requires cleanup after migration

- **GIVEN** consumers and tests have been migrated to the narrowed public boundary and standalone verification ownership split
- **WHEN** completion of the extraction change is evaluated
- **THEN** stale transitional public surface and compatibility code for superseded diagnostics-session or pass-capture behavior SHALL have been removed, internalized, or reassigned
- **AND** the change SHALL NOT be considered complete while those stale transitional elements still ship as supported caller-facing boundary
