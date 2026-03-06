## Context

`goggles-filter-chain` currently exists as an in-repo boundary target, but practical publication is still coupled to monorepo wiring:

- build orchestration and dependency discovery assume monorepo CMake/Pixi context,
- shared utility code is consumed from `src/util` without a narrow external contract,
- release and ABI validation flow is not defined as a standalone library lifecycle.

Milestone 1 requires publishing the filter runtime as an independent repository while preserving host behavior and C API compatibility.

## Goals / Non-Goals

**Goals:**

- Extract and publish the runtime boundary (`chain + shader + texture`) as a standalone repository.
- Preserve C API compatibility for ABI v1 consumers.
- Introduce a reusable `util-core` dependency boundary for shared utility contracts needed by the standalone runtime.
- Define a reproducible standalone build/test/release gate and host-consumption path.

**Non-Goals:**

- Reworking host-side app/UI/compositor architecture.
- Changing render stage semantics, ordering contracts, or control contracts unrelated to packaging/extraction.
- Delivering a full long-term multi-repo platform strategy beyond milestone 1 publication readiness.

## Decisions

### Decision 1: Runtime extraction boundary is `chain + shader + texture`

- **Decision**: The standalone repository owns runtime modules equivalent to current `src/render/chain`, `src/render/shader`, and `src/render/texture`.
- **Rationale**: This matches the actual runtime ownership boundary and avoids artificial chain-only shims that duplicate runtime internals.
- **Alternative considered**: Chain-only extraction with bridged shader/texture ownership.
- **Why not chosen**: It increases coupling and delays true standalone behavior.

### Decision 2: Shared utilities move behind `util-core`

- **Decision**: Shared utility usage required by extracted runtime is consumed through a versioned `util-core` library contract.
- **Rationale**: It preserves reuse across host and standalone runtime without duplicating utility logic or retaining broad monorepo coupling.
- **Alternative considered**: Vendoring utility snippets into standalone repository.
- **Why not chosen**: High drift risk and duplicated maintenance burden.

### Decision 3: C API compatibility is enforced by explicit release gates

- **Decision**: Standalone releases require ABI/API checks for `goggles_filter_chain.h` against baseline v1 contracts.
- **Rationale**: C API stability is a hard constraint and must be mechanically verified across releases.
- **Alternative considered**: Relying on manual API review and unit tests only.
- **Why not chosen**: Insufficient protection against subtle ABI drift.

### Decision 4: Migration is phased with reversible handoff points

- **Decision**: Use ordered phases (contract prep -> util-core split -> extraction -> host rewire -> release hardening) with clear rollback points.
- **Rationale**: Limits destabilization risk in host runtime and enables narrow validation after each phase.
- **Alternative considered**: One-shot extraction and host flip.
- **Why not chosen**: High blast radius and difficult failure isolation.

### Decision 5: Milestone 1 release profile is fixed

- **Decision**: Milestone 1 publishes Linux `x86_64` artifacts only, with required validation across clang and gcc static/shared build lanes.
- **Rationale**: Current project platform baseline is Linux and existing smoke lanes already validate static/shared with both toolchains.
- **Alternative considered**: Multi-platform release in milestone 1.
- **Why not chosen**: Expands risk surface before extraction contract hardening.

### Decision 6: Artifact trust and channel model is fail-closed

- **Decision**: Publication and consumption require integrity/provenance evidence (checksums + signature/attestation metadata) and approved-channel allowlisting.
- **Rationale**: External artifact consumption introduces supply-chain risk; trust checks must block host flip when validation fails.
- **Alternative considered**: Trust via version pinning only.
- **Why not chosen**: Pinning alone does not authenticate publisher identity or artifact integrity.

## Critical Digest Resolution

| Critical digest item | Resolution in artifacts |
| --- | --- |
| Preserve C API stability | `specs/filter-chain-c-api/spec.md` (Standalone Release ABI Guardrail), `tasks.md` section 3 |
| Boundary is chain+shader+texture | `specs/goggles-filter-chain/spec.md` (Standalone Repository Ownership Boundary), `tasks.md` sections 1-2 |
| util-core split strategy | `specs/goggles-filter-chain/spec.md` (Versioned util-core Dependency Contract), `specs/dependency-management/spec.md`, `tasks.md` section 1 |
| Build + tests + versioned artifact gate | `specs/goggles-filter-chain/spec.md` (Standalone Publication Readiness Gate), `tasks.md` sections 2, 5 |
| Migration fallback when util-core blocks | `Migration Plan` + rollback strategy in this design and `tasks.md` section 4 |
| Compatibility evidence + reproducible release quality bars | `specs/goggles-filter-chain/spec.md` + `specs/filter-chain-c-api/spec.md`, `tasks.md` section 3 |
| Artifact channels and platform/toolchain matrix are explicit | `Decisions` in this design + `specs/goggles-filter-chain/spec.md` + `tasks.md` section 2 |
| Artifact trust/authenticity controls | `specs/dependency-management/spec.md` + `tasks.md` sections 3-5 |

## Risks / Trade-offs

- **[Risk] util-core scope creep** -> **Mitigation**: define allowed utility surface up front and enforce include ownership checks.
- **[Risk] ABI drift across standalone releases** -> **Mitigation**: add release-blocking compatibility checks and version-policy validation.
- **[Risk] Host integration regressions during rewire** -> **Mitigation**: keep boundary adapter layer stable and run host integration smoke checks per phase.
- **[Risk] Packaging complexity increases CI time** -> **Mitigation**: separate fast PR checks from full release matrix while preserving required release gates.

## Migration Plan

1. Define normative boundary and compatibility requirements (spec deltas).
2. Carve out `util-core` contract and migrate runtime dependencies onto it.
3. Materialize standalone repository build/release pipeline for runtime modules.
4. Rewire host dependency consumption to standalone artifacts with dual-path fallback during transition.
5. Enable release hardening gates (ABI checks, artifact verification, integration smoke).

**Rollback strategy**:

- Keep host-side fallback to in-tree boundary until standalone release and compatibility gates are green.
- If release or trust gates fail, block host flip and continue consuming in-tree path until fixed.
- Fallback and migration toggles MUST only allow vetted sources (in-tree boundary or pinned+verified standalone artifacts).

## Open Questions

- None for milestone-1 contract gating; unresolved execution details should be tracked as implementation notes during apply.
