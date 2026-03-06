## 1. Define extraction and utility contracts

- [x] 1.1 Define the standalone runtime ownership boundary for `chain + shader + texture` and map current monorepo file ownership.
- [x] 1.2 Define `util-core` API surface needed by extracted runtime modules and list excluded host-only utilities.
- [x] 1.3 Add/update build-facing contract headers and include-boundary checks for runtime vs host ownership.
- [x] 1.4 Produce a dependency-gap report (runtime deps, util-core deps, excluded host-only deps) at `reports/filter-chain-standalone/dependency-gap.md` and register it as a release-gate input artifact.
- [x] 1.5 Publish util-core ownership and version-policy artifacts (machine-readable + human-readable) and verify consumer compatibility constraints consume them.

## 2. Standalone runtime build and packaging wiring

- [x] 2.1 Create standalone CMake target graph for extracted runtime modules with no backend/app/ui/compositor linkage.
- [x] 2.2 Wire dependency resolution for standalone runtime and util-core with pinned, reproducible version constraints.
- [x] 2.3 Add standalone artifact packaging metadata and release outputs for static/shared runtime deliverables.
- [x] 2.4 Define standalone CI workflows with required status checks for build, test, and release dry-run.
- [x] 2.5 Validate milestone-1 artifact manifest includes source bundle, public headers, static/shared runtime libraries, and consumer package metadata outputs.

## 3. C API stability and release gate enforcement

- [x] 3.1 Add ABI/API compatibility checks for `goggles_filter_chain.h` against v1 baseline in release workflow.
- [x] 3.2 Ensure packaged include/export surface matches declared ABI/version contract.
- [x] 3.3 Add release-blocking checks so publication fails on compatibility or required gate failures.
- [x] 3.4 Generate and archive required compatibility evidence artifacts (ABI diff report, exported-symbol manifest diff, header contract check output).
- [x] 3.5 Generate and publish artifact integrity/provenance outputs (checksums + signature/attestation metadata) and fail release on verification errors.
- [x] 3.6 Wire compatibility evidence references into machine-readable package metadata/manifest and verify host intake checks consume them.

## 4. Host integration migration and fallback

- [x] 4.1 Rewire host runtime dependency consumption to standalone filter runtime and util-core artifacts behind boundary-safe interfaces.
- [x] 4.2 Implement phased migration toggles/fallback so host can remain on in-tree path until release gates are green.
- [x] 4.3 Verify host backend integration behavior remains aligned with existing runtime stage/order contracts.
- [x] 4.4 Define util-core blocker fallback triggers and re-entry criteria for phased migration, constrained to vetted sources only (in-tree path or pinned+verified standalone artifacts).
- [x] 4.5 Add dual-path validation that exercises standalone mode and fallback mode before host flip.

## 5. Validation and OpenSpec alignment

- [x] 5.1 Run required verification commands: `pixi run build -p asan`, `pixi run test -p asan`, `pixi run build -p quality`.
- [x] 5.2 Run focused compatibility checks for filter runtime C API and standalone artifact validation.
- [x] 5.3 Update impacted spec/delta artifacts if implementation decisions require contract clarifications before apply handoff.
- [x] 5.4 Validate standalone CI required status checks and release dry-run outputs are fail-closed.
- [x] 5.5 Validate lockfile authority and drift checks for host and standalone workflows; fail on unlocked dependency resolution.
- [x] 5.6 Validate channel allowlist enforcement for publication and host dependency intake.
- [x] 5.7 Define and validate version-controlled approved-channel allowlist policy used by CI trust gates.
- [x] 5.8 Add Pixi task entrypoint `verify-policy-artifacts` and run policy-artifact validation checks for util-core ownership/version policy and channel allowlist trust constraints.
- [x] 5.9 When syncing to living specs, update `## Purpose` in affected living specs currently marked `TBD`.

## 6. Requirement Traceability

| Requirement | Task IDs | Verification commands |
| --- | --- | --- |
| Standalone Repository Ownership Boundary | 1.1, 1.3, 4.1 | `pixi run build -p asan`; `ctest --preset asan -R "goggles_unit_tests" --output-on-failure` |
| Versioned util-core Dependency Contract | 1.2, 1.4, 4.1 | `pixi run build -p asan`; `pixi run test -p asan` |
| Standalone Publication Readiness Gate | 2.4, 3.3, 3.4, 5.4 | `pixi run build -p quality`; `pixi run test -p asan` |
| Milestone-1 Release Profile and Artifact Manifest | 2.3, 2.5, 5.4 | `pixi run build -p asan`; `pixi run test -p asan` |
| Standalone Release ABI Guardrail | 3.1, 3.3, 3.4 | `pixi run build -p quality`; `ctest --preset asan -R "goggles_unit_tests" --output-on-failure` |
| Public Header Packaging Contract | 3.2, 2.5, 3.6 | `pixi run build -p asan`; `pixi run build -p quality` |
| Consumer Package Metadata Contract | 2.3, 3.6, 5.2 | `pixi run build -p quality`; `pixi run test -p asan` |
| Standalone Filter Runtime Dependency Pinning | 2.2, 5.5 | `pixi run build -p asan`; `pixi run test -p asan` |
| Lockfile Authority and Drift Enforcement | 2.2, 5.5 | `pixi run build -p asan`; `pixi run build -p quality` |
| util-core Contract Separation | 1.2, 1.4, 4.1 | `pixi run build -p asan`; `pixi run test -p asan` |
| util-core Ownership and Version Policy | 1.5, 5.8 | `pixi run build -p quality` |
| Standalone Workflow Policy Compliance | 2.4, 5.1, 5.4 | `pixi run build -p asan`; `pixi run test -p asan`; `pixi run build -p quality` |
| Artifact Integrity and Channel Allowlisting | 3.5, 5.6, 5.7, 5.8 | `pixi run build -p quality`; `pixi run test -p asan` |
