# Proposal: Standalone Filter-Chain API

## Intent

Redesign `filter-chain` as a reusable standalone library with a clean public contract that Goggles
consumes as one host, rather than as an extension of Goggles internals. The current surface mixes
host concerns, Goggles-specific support assumptions, and evolving ABI decisions; this change
establishes a clean-slate public interface that is easier to embed, package, test, and evolve.

## Problem

The existing filter-chain boundary still reflects in-repo Goggles integration choices instead of a
package-first library contract. Public naming, lifecycle, logging, asset lookup, Vulkan
integration, packaging metadata, and supporting docs/examples are not yet aligned around an
external consumer model, which makes standalone reuse, FFI adoption, and installed-package
verification harder than necessary.

The intended end state is a complete public cutover, not a staged compatibility outcome. When this
change is finished, the shipped standalone package must contain only the new clean-slate contract:
no deprecated APIs, no compatibility aliases or shims, no dual public surfaces, no stale
install-only runtime assumptions, and no stale docs/examples that reference removed names or
contracts.

## Scope

### In Scope
- Define a clean-slate standalone public model split around `instance`, `device`, `program`, and
  `chain` responsibilities.
- Define a first-class C API with the `goggles_fc_*` prefix and clean host/library ownership rules.
- Redesign Vulkan host integration so the host provides device/queue handles and per-record inputs
  while the library owns compilation, caches, pipelines, upload/setup command resources, and
  intermediate runtime resources.
- Replace shared/global logging assumptions with host-routable callback or sink-based diagnostics.
- Define library-owned embedded/internal shader and asset handling plus preset loading from file and
  memory sources.
- Keep Goggles as a consumer of the standalone package through its render/backend controller
  integration rather than as the source of its public abstractions or a user of standalone
  internals.
- Remove legacy public headers, symbols, wrapper shapes, build/install/export wiring, examples,
  tests, and package metadata that would preserve or imply the old contract after cutover.
- Update the change artifacts so they describe the final shipped contract rather than an
  intermediate migration state.

### Out of Scope
- Source or ABI compatibility with the current filter-chain public naming or handle model.
- Non-Vulkan backends in v1.
- A compatibility layer, deprecated aliases, or migration shims for old `goggles_chain_*`/`fc_*`
  style entry points.
- Final implementation details for every subsystem; those belong in follow-on spec, design, and
  task artifacts.

## Non-goals

- Preserve current public naming when a cleaner standalone contract is available.
- Make Goggles application types, config objects, or helper utilities part of the library surface.
- Require consumers to provide shader directories or other Goggles-repository-relative assets.

## Approach

Adopt a package-first API redesign centered on explicit host/library boundaries. The host owns
process integration concerns such as Vulkan instance/device selection, queue ownership, submission,
presentation, and per-record source/target inputs. The standalone library owns reusable runtime
concerns such as preset parsing, shader compilation/reflection, internal caches, stage pipelines,
setup/upload command helpers, intermediate images/buffers, and embedded built-in assets.

The public contract will prioritize a stable C surface first, with naming and lifecycle rebuilt
 around `goggles_fc_*` entry points and object families that make FFI and downstream embedding
 straightforward. Logging and diagnostics will route outward through callbacks/sinks so the library
 does not expose or depend on shared/global `spdlog` symbols. Goggles integration will remain on
 the consumer side of the package boundary through render/backend controller code that maps
 existing runtime/backend needs onto the standalone contract.

The implementation is only complete once all legacy public traces are removed from the shipped
surface: no retained `goggles_chain_*` or `fc_*` entry points, no deprecated wrapper header or
namespace, no install/export logic for removed surfaces, no public `shader_dir` or similar stale
runtime assumptions, and no docs/examples/tests that continue to teach the removed contract.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `filter-chain/include/` | Modified | Replace or reorganize the standalone public header surface around the new C API and object model. |
| `filter-chain/src/` | Modified | Realign internal ownership, logging, asset packaging, Vulkan boundary handling, and runtime lifecycle to match the standalone contract. |
| `filter-chain/tests/` | Modified | Shift contract coverage toward installed-surface and standalone-owned API validation. |
| `filter-chain/CMakeLists.txt` | Modified | Ensure packaging/export/install rules match the redesigned public interface. |
| `filter-chain/cmake/` | Modified | Remove stale exported package metadata and install-time assumptions that describe the old contract. |
| `src/render/` | Modified | Adapt Goggles runtime/backend integration to consume the standalone API through the render/backend controller boundary. |
| package-facing docs/examples | Modified | Remove or rewrite docs/examples that reference removed names, wrapper shapes, or asset assumptions. |
| `openspec/specs/filter-chain-c-api/spec.md` | Modified | Update the C ABI contract to the new naming, lifecycle, and ownership model. |
| `openspec/specs/filter-chain-cpp-wrapper/spec.md` | Modified | Recast the wrapper as a thin C++ consumer layer over the redesigned standalone C API. |
| `openspec/specs/goggles-filter-chain/spec.md` | Modified | Update host/library responsibility boundaries and Goggles-consumer expectations. |
| `openspec/specs/filter-chain-assets-package/spec.md` | Modified | Align asset ownership and preset source expectations with embedded/library-owned assets. |

## Policy-Sensitive Impacts

- Error handling: public failures stay explicit and non-exception-based, but error and diagnostic
  transport move to host-routable callbacks/sinks.
- Logging: standalone code must stop depending on shared/global Goggles logging state or exported
  `spdlog` symbols.
- Threading: the proposal keeps host/library boundaries explicit so threading guarantees can be
  specified without hidden shared-state assumptions.
- Vulkan split: v1 stays Vulkan-only, but host-provided handles and library-owned runtime resources
  must remain clearly separated.
- Lifetime/ownership: object model and API calls must make borrowed vs owned resources obvious for
  FFI consumers and Goggles render/backend integration.

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Clean-slate API scope grows into an uncontrolled refactor | Medium | Lock the next spec/design work to public-boundary decisions first and defer unrelated internals. |
| Goggles integration uncovers hidden assumptions about current runtime ownership | High | Capture controller-side consumer requirements explicitly in design and preserve Goggles as a consumer, not a boundary exception. |
| Logging/diagnostic redesign creates unclear host callback lifetime rules | Medium | Specify callback registration, threading, and ownership semantics in the C API spec before implementation. |
| Embedded assets and memory/file preset sources complicate packaging | Medium | Define asset resolution and packaging behavior in specs and validate against installed-package tests. |

## Rollback Plan

If the redesign proves unworkable, revert the standalone API changes within `filter-chain/`, keep
Goggles on the current in-repo integration path, and discard the controller-side consumer
integration changes before syncing any delta specs into living specs. Because backward
compatibility is explicitly out of scope, the rollback boundary is the change branch itself rather
than a compatibility preservation layer.

## Dependencies

- Follow-on delta specs for the standalone C API, C++ wrapper, Goggles integration boundary, and
  asset package behavior.
- A technical design that defines object lifetimes, host callbacks, Vulkan handle flow, and
  Goggles render/backend controller integration strategy.

## Validation Plan

- Validate that the proposal produces spec updates covering C API naming, lifecycle, ownership,
  logging/diagnostics, asset sourcing, and Goggles host responsibilities.
- Verify the redesigned package can be built, installed, and consumed without Goggles-private
  headers, repository-relative assets, or shared global logging state.
- Verify no shipped public header, example, test target, install/export rule, or package metadata
  path still exposes removed names, compatibility shims, or install-only asset assumptions.
- Verify Goggles remains able to integrate through render/backend controller code rather than
  direct dependence on standalone internals.

## Success Criteria

- [x] The change artifacts define a standalone public model centered on `instance`, `device`,
  `program`, and `chain` responsibilities.
- [x] The planned C API is first-class, uses the `goggles_fc_*` prefix, and does not require a
  compatibility layer.
- [x] The resulting specs make host/library ownership, Vulkan boundaries, logging sinks, and asset
  sourcing explicit and testable.
- [x] Goggles is positioned as a consumer of the standalone package, not as the source of its public
  interface assumptions.
- [x] The final intended shipped state is unambiguous: no legacy public surface, no deprecated or
  dual public contract, no stale runtime/package assumptions, and no stale docs/examples left
  behind.
