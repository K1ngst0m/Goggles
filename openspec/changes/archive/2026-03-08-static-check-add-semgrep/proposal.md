# Change: Add Repo-Controlled Semgrep to the Static Analysis Workflow

## Why
The current static-analysis lane only enforces the Pixi-managed `build -p quality` flow, which leaves several high-signal policy bans dependent on manual review even though they are pattern-checkable. That gap makes enforcement inconsistent and slows review on rules that should fail deterministically before humans look at the patch.

The requested change adds a repo-controlled Semgrep gate that complements, rather than replaces, the existing clang-tidy and build/test gates. It keeps the scan narrow so Semgrep only owns policy clauses it can enforce reliably.

## Problem
- The repository has no Semgrep configuration, task entrypoint, or CI gate today.
- Several policy bans in `docs/project_policies.md` are straightforward static-pattern checks, but they are not enforced consistently by the current toolchain.
- Without a shared repo-controlled entrypoint, local and CI behavior would drift if Semgrep were added ad hoc.

## Scope
- Add a checked-in Semgrep ruleset and repository-controlled config.
- Add the Pixi-managed `pixi run semgrep` entrypoint so local and CI usage share the same tool and config surface.
- Add Semgrep as a blocking step in the existing static-analysis workflow without removing the current `pixi run build -p quality` gate.
- Limit Semgrep to high-signal policy bans that are realistically Semgrep-enforceable.

## What Changes
- Add a repo-controlled Semgrep ruleset for approved high-signal policy bans.
- Extend the static-analysis workflow to run Semgrep as a blocking CI gate.
- Provide the deterministic local `pixi run semgrep` entrypoint that uses the same checked-in config and rules as CI.
- Add tracked verification inputs that prove each claimed Semgrep-enforced policy clause both matches representative violating code and does not flag approved in-scope or exception cases.
- Admit Semgrep through Pixi source-of-truth files, keeping `pixi.toml` and `pixi.lock` in sync.
- Scope Semgrep to policy-derived checks such as banned logging APIs, banned ownership patterns, banned Vulkan wrapper usage, and banned render-path thread primitives.
- Keep formatting, naming, include ordering, lockfile/preset checks, and runtime-validation rules with their existing tools instead of duplicating them in Semgrep.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `ci`: static-analysis behavior changes to include a blocking, repo-controlled Semgrep gate alongside the existing quality build.
- `build-system`: repository tooling changes to provide deterministic Semgrep execution and checked-in rule sources for both local and CI use.

## Non-Goals
- Replacing `clang-format`, `clang-tidy`, preset builds, or test gates with Semgrep.
- Using Semgrep registry defaults, hosted defaults, or any non-repository rule source.
- Turning Semgrep into a broad generic lint pass for every tool-enforceable policy clause.
- Encoding review-only, runtime-only, or heavily semantic policy rules into static pattern checks.

## Risks
- Over-broad rules could create noisy failures and reduce trust in the gate.
- Poor path scoping could flag allowed code in `src/capture/vk_layer/` or other exceptions where policy differs by subsystem.
- CI runtime will increase if the scan is not kept targeted.

## Dependency Admission
- Rationale: Semgrep is admitted to close a deterministic enforcement gap for high-signal policy bans that are currently review-only in practice.
- License compatibility check: the change MUST confirm the Semgrep package/license is acceptable for repository use before apply is complete.
- Maintenance assessment: the change MUST confirm the selected Semgrep package source is actively maintained enough for a blocking CI gate.
- Team agreement: the dependency addition MUST be explicit in code review before apply is considered complete.

## Validation Plan
- `openspec validate static-check-add-semgrep --strict`
- update `pixi.lock` in sync with `pixi.toml`
- `pixi run bash -c 'command -v semgrep && semgrep --version'`
- verify each claimed policy clause has tracked positive and negative Semgrep verification inputs checked into the repository
- verify positive inputs include representative repo-style patterns for the targeted code shape, not only toy single-line snippets
- verify scoped rules cover the directories they claim to govern and do not flag documented exception paths
- `pixi run semgrep`
- `pixi run build -p quality`
- confirm CI logs the Semgrep path provenance and version surface before the blocking scan runs
- confirm task success is based on the claimed rule coverage passing those verification inputs, not only on the aggregate Semgrep command exiting successfully

## Impact
- Affected specs:
  - `ci`
  - `build-system`
- Affected files/modules (expected):
  - `.github/workflows/ci.yml`
  - `pixi.toml`
  - `.semgrep.yml`
  - `.semgrep/rules/`
  - tracked Semgrep verification inputs under `tests/`
- Policy-sensitive impacts:
  - logging, ownership, Vulkan API split, Vulkan lifetime helpers, and render-path threading are touched only as static rule sources
  - no runtime behavior or subsystem ownership model changes are intended
