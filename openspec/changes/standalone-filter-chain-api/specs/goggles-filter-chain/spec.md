# Delta for goggles-filter-chain

## MODIFIED Requirements

### Requirement: Complete Filter Runtime Ownership Boundary

The standalone filter library SHALL own preset parsing, shader compilation/reflection, internal
caches, stage pipelines, setup/upload command helpers, intermediate images and buffers, and
library-owned built-in assets needed for normal runtime behavior. Goggles SHALL remain responsible
only for host integration concerns outside that boundary.

#### Scenario: Goggles uses the standalone runtime as a consumer

- GIVEN Goggles integrates the standalone filter library into its render pipeline
- WHEN it creates and uses filter runtime objects
- THEN the standalone library SHALL own reusable filter runtime internals and built-in assets
- AND Goggles SHALL consume those services without becoming the source of the library's public abstractions

### Requirement: Host Backend Responsibility Boundary

The host backend SHALL remain responsible for Vulkan instance and device selection, queue ownership,
submission, presentation, swapchain lifecycle, external image import, synchronization, and per-record
source/target bindings. The standalone library SHALL consume those host-provided inputs through
explicit contracts and SHALL NOT assume implicit control over host submission flow.

The completed Goggles integration MUST NOT retain stale bypass paths or compatibility assumptions
from the pre-redesign contract. In particular, Goggles MUST NOT remain a hidden contract owner for
legacy public types, old asset-directory expectations, or alternate direct-internal integration
paths once the standalone consumer cutover is complete.

#### Scenario: Goggles records with explicit host/library separation

- GIVEN Goggles has already prepared the Vulkan command buffer and host-owned source/target resources
- WHEN it invokes standalone chain recording for a frame
- THEN Goggles SHALL provide the borrowed per-record inputs required by the public contract
- AND the standalone library SHALL record only its library-owned rendering work without taking over submission or presentation

### Requirement: Goggles Integration Leaves No Legacy Contract Path

Goggles SHALL consume the standalone library only through the final standalone boundary or approved
thin wrapper layered over it. When the migration is complete, Goggles-facing docs, adapters, tests,
and build wiring MUST NOT preserve alternate public stories based on removed `goggles_chain_*`
contracts or install-only asset assumptions.

The `FILTER_CHAIN_ENABLE_LEGACY_API` CMake gating mechanism, all legacy `goggles_chain_*` C API
files, the legacy `goggles_filter_chain.hpp` C++ wrapper, and the internal
`goggles_chain_legacy.h` header have been removed. The Goggles application consumes the standalone
library through `FilterChainController`, which owns the C++ RAII wrapper object graph over the
`goggles_fc_*` C ABI. No Goggles-side source code references `FilterChainRuntime`, `shader_dir`,
`goggles_chain_*`, or standalone internals.

#### Scenario: Goggles cutover removes alternate contract ownership

- GIVEN the standalone redesign has fully landed
- WHEN maintainers inspect Goggles integration points and package-facing references
- THEN Goggles SHALL appear only as a consumer of the standalone contract
- AND no Goggles-owned docs, tests, or exported integration surface SHALL preserve the removed public contract

### Requirement: Boundary-safe VulkanContext Contract Placement

Boundary-owned Vulkan context contracts SHALL reflect the standalone object model and SHALL expose
only the host-provided information required to create standalone instance, device, program, or chain
objects. Goggles integration headers SHALL consume those standalone-owned contracts rather than
defining alternative public types.

#### Scenario: Goggles controller integration includes standalone-owned boundary contracts

- GIVEN Goggles implements controller-side consumer integration with the standalone library
- WHEN controller headers describe Vulkan initialization inputs
- THEN those headers SHALL consume standalone-owned contract types and naming
- AND they SHALL NOT redefine the public host/library boundary around Goggles-private abstractions

### Requirement: Error Model and Diagnostics Contract

Goggles integration SHALL route standalone diagnostics through host-facing callback or sink
registration rather than through shared/global logger symbols. Consumer integration code MAY
translate or enrich messages at subsystem boundaries, but it SHALL preserve the standalone library's host-routable
diagnostic model.

#### Scenario: Goggles forwards standalone diagnostics into app logging

- GIVEN Goggles configures a standalone logging callback or sink during integration
- WHEN the standalone library emits diagnostics during preset load or frame recording
- THEN Goggles SHALL receive those diagnostics through the documented callback path
- AND the integration SHALL NOT require shared global logging state between Goggles and the library

### Requirement: Library-Owned Support Boundary

The standalone library's public and internal support contracts SHALL be defined independently of
Goggles-private support code. Goggles-specific consumer integration MUST treat the library as an
external consumer-facing package and SHALL NOT rely on special-case access to define or reinterpret
the public contract.

The installed standalone package MAY ship shared infrastructure headers (such as
`FilterControlDescriptor`, `VulkanContext`, and the `goggles::ScaleMode` enum) alongside the
`goggles_fc_*` C API surface, provided those headers are self-contained (no Goggles `util/` or
application dependencies). These shared types serve as standalone library-owned contracts consumed by
both the library and any host (including Goggles). The standalone library's public `goggles::ScaleMode`
enum defines five values — `fit`, `fill`, `stretch`, `integer`, and `dynamic` — which correspond
one-to-one with the C API constants `GOGGLES_FC_SCALE_MODE_FIT` through
`GOGGLES_FC_SCALE_MODE_DYNAMIC`. All five scale modes are part of the standalone library's public
surface and are available to any consumer.

#### Scenario: Goggles consumer integration does not redefine the standalone API contract

- GIVEN Goggles and another downstream host both consume the standalone package
- WHEN their integration layers are compared
- THEN both SHALL rely on the same standalone public contract for object ownership and lifecycle
- AND Goggles-specific controller integration SHALL NOT add hidden contract assumptions unavailable to other consumers

## REMOVED Requirements

### Requirement: In-Repo Subdirectory Bridge During Extraction

(Reason: This change's public contract centers on Goggles as a consumer rather than on a
transitional in-repo integration mechanism. Build-bridge details belong in design or packaging work,
not in the host boundary spec.)
