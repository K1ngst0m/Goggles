# Delta for filter-chain-cpp-wrapper

## MODIFIED Requirements

### Requirement: C++20 Wrapper Header for Filter Chain

The standalone project SHALL provide a thin C++20 consumer wrapper over the standalone C API rather
than a Goggles-specific integration header. The wrapper SHALL mirror the standalone object model and
installed package surface, and it SHALL remain consumable without depending on Goggles-private
headers or backend-only types.

The completed wrapper surface MUST NOT leave a second public C++ contract behind. Deprecated wrapper
headers, compatibility namespaces, and package-facing examples for removed wrapper shapes MUST be
deleted or rewritten so the installed package documents only the thin wrapper over the `goggles_fc_*`
C API.

#### Scenario: External C++ consumer includes the wrapper

- GIVEN an external C++ consumer integrates the installed standalone package
- WHEN it includes the wrapper header
- THEN the wrapper SHALL present a standalone consumer-facing API over the C layer
- AND the include path SHALL NOT require Goggles backend or application headers

#### Scenario: Installed package ships only the final wrapper contract

- GIVEN a maintainer inspects installed wrapper headers and package-facing examples
- WHEN the standalone redesign is complete
- THEN only the `goggles::filter_chain` wrapper layout SHALL be documented and shipped for normal consumer use
- AND legacy wrapper headers, compatibility aliases, or stale examples for removed contracts SHALL NOT remain public

### Requirement: RAII Ownership for Chain Handle

RAII ownership in the C++ wrapper MUST align with the standalone object families introduced by the C
API. Wrapper-owned objects for instance, device, program, and chain roles MUST destroy the
underlying C handles exactly once and MUST preserve explicit ownership boundaries between host-
provided Vulkan context and library-owned runtime objects.

#### Scenario: Wrapper releases standalone objects in dependency order

- GIVEN a C++ consumer owns wrapper objects created from the standalone package
- WHEN those wrapper objects leave scope
- THEN each underlying standalone C handle SHALL be destroyed exactly once in a valid dependency order
- AND host-owned Vulkan handles SHALL remain under host ownership rather than being implicitly destroyed by the wrapper

### Requirement: Strongly Typed C++ Runtime Surface

The C++ wrapper SHALL expose typed operations for the standalone public model, including program and
chain lifecycle, preset loading from file and memory sources, and dedicated chain update helpers for
operations that remain distinct in the C ABI such as prechain-resolution changes. Normal wrapper
usage SHALL NOT require callers to manage legacy Goggles naming, deprecated compatibility flows, or
public `shader_dir` configuration.

The wrapper's `goggles::filter_chain::ScaleMode` enum SHALL mirror the C API `GOGGLES_FC_SCALE_MODE_*`
constant values exactly (`stretch=0`, `fit=1`, `integer=2`, `fill=3`, `dynamic=4`). This wrapper
enum SHALL define the standalone public scale-mode contract directly, and wrapper consumers SHALL be
able to select every public C API scale mode without falling back to raw integer constants.

#### Scenario: Wrapper loads a preset from memory

- GIVEN a C++ consumer has preset bytes in memory
- WHEN it invokes the wrapper's preset-loading API
- THEN the wrapper SHALL expose a typed memory-source load path over the standalone C API
- AND the callsite SHALL NOT need filesystem-only temporary paths or a public shader directory parameter

#### Scenario: Wrapper updates prechain resolution through a dedicated helper

- GIVEN a C++ consumer already owns a live wrapper `Chain`
- WHEN it updates prechain resolution after creation
- THEN the wrapper SHALL expose a dedicated `set_prechain_resolution(...)` operation over the standalone C API
- AND the callsite SHALL NOT need to overload resize or recreate the chain to express that change

#### Scenario: Wrapper selects fill or dynamic scaling without raw constants

- GIVEN a C++ consumer prepares record-time inputs for the standalone wrapper contract
- WHEN it chooses `goggles::filter_chain::ScaleMode::fill` or `goggles::filter_chain::ScaleMode::dynamic`
- THEN those enum values SHALL be valid public wrapper choices
- AND their numeric values SHALL match the corresponding `GOGGLES_FC_SCALE_MODE_*` constants exactly

### Requirement: Result-Based Error Propagation

Every fallible wrapper operation SHALL continue to use project-style result objects for expected
failures, but diagnostics emitted by the standalone library SHALL remain host-routable rather than
being coupled to Goggles logging internals.

#### Scenario: Wrapper consumer forwards standalone diagnostics

- GIVEN a wrapper consumer registers a host logging callback or sink
- WHEN an expected runtime failure occurs in the standalone library
- THEN the wrapper SHALL return a failed result to the caller
- AND library diagnostics SHALL remain routable through the configured host-facing callback path

### Requirement: Boundary Isolation

The C++ wrapper SHALL act as a consumer convenience layer over the standalone C ABI and SHALL NOT
define the public contract independently of that C surface. Goggles-specific adapters MAY consume the
wrapper, but the wrapper's semantics MUST remain valid for non-Goggles consumers.

#### Scenario: Goggles consumes the same wrapper contract as other hosts

- GIVEN Goggles integrates the standalone library through an adapter layer
- WHEN it uses the C++ wrapper
- THEN Goggles SHALL consume the same standalone wrapper contract available to other hosts
- AND no Goggles-only behavior SHALL be required to make the wrapper API meaningful

### Requirement: Wrapper Surface Has No Legacy Compatibility Layer

The final standalone C++ wrapper contract MUST be singular and clean-slate. The shipped wrapper MUST
NOT preserve deprecated adapter helpers, namespace aliases, or old header layouts merely to ease
migration from pre-redesign public surfaces.

#### Scenario: Consumer cannot accidentally adopt removed wrapper names

- GIVEN a downstream C++ consumer follows the installed wrapper docs and examples
- WHEN it integrates the completed standalone package
- THEN it SHALL encounter only the final wrapper names and header layout
- AND it SHALL NOT be taught or encouraged to use removed wrapper contracts through compatibility helpers or deprecation notices

## REMOVED Requirements

### Requirement: C ABI Continuity During Migration

(Reason: The standalone redesign is intentionally clean-slate, so preserving the prior `goggles_chain_*`
C ABI behavior is not a v1 requirement for this change.)
