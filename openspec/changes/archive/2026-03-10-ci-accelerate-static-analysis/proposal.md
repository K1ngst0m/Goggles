## Why

The current CI static-analysis coverage runs Semgrep and the quality build in one serialized lane,
which dominates workflow wall-clock time and slows pull-request feedback.

This change restructures CI static-analysis execution to preserve equivalent PR-gated safety while
reducing end-to-end latency and retaining reproducible local entrypoints.

## Problem

- The `Static Analysis` PR check is the longest CI job and extends total workflow completion time.
- Semgrep and quality build checks are currently composed as one lane, preventing job-level
  parallelism.
- Local reproduction relies on `pixi run ci`, so any CI reshaping must preserve deterministic
  repository-controlled command surfaces.

## Scope

- Restructure static-analysis execution so Semgrep and quality-build checks can run as independent
  PR-gated jobs.
- Preserve equivalent PR coverage: both checks remain required before merge.
- Keep local reproduction aligned through `pixi run ci` lane entrypoints.
- Update OpenSpec CI requirements, design rationale, and implementation tasks to reflect the new
  contract.

## What Changes

- Split static-analysis execution into two independently runnable lanes: Semgrep-focused and
  quality-build-focused.
- Update GitHub Actions CI job graph so those lanes run in parallel as separate required PR checks.
- Keep a combined static-analysis local entrypoint that executes both checks for deterministic local
  reproduction.
- Preserve existing policy and quality semantics: Semgrep remains blocking and quality build remains
  blocking.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `ci`: static-analysis requirements change from serialized lane composition to parallelizable,
  independently runnable lanes with equivalent PR-gated coverage and preserved local reproduction
  semantics.

## Non-goals

- Reducing PR safety coverage by moving required checks to non-PR triggers.
- Replacing Semgrep or quality build with weaker substitutes.
- Broad CI redesign outside static-analysis composition and its local/CI entrypoints.
- Defining a hard SLA runtime target; this remains best-effort optimization.

## Impact

- Affected modules/files:
  - `.github/workflows/ci.yml`
  - `scripts/task/ci.sh`
  - `pixi.toml`
  - optional CI helper docs if command surfaces change
- Impacted OpenSpec specs:
  - `openspec/specs/ci/spec.md`
- No product runtime, Vulkan, shader, packaging, or API behavior changes are expected.

## Policy-sensitive impacts

- Build/test orchestration MUST continue using Pixi task entrypoints and CMake/CTest preset-based
  commands.
- Static-analysis checks remain blocking gates in PR.
- Repository-controlled determinism MUST be preserved for local and CI execution.

## Risks

| Risk | Severity | Likelihood | Mitigation |
|------|----------|------------|------------|
| PR coverage accidentally weakens during job split | HIGH | MEDIUM | Require both Semgrep and quality jobs as blocking PR checks |
| Local reproduction drifts from CI behavior | MEDIUM | MEDIUM | Keep `pixi run ci` as canonical entrypoint with explicit lane mapping |
| Cache behavior changes offset expected speedup | MEDIUM | MEDIUM | Keep ccache configuration explicit and measure stage timing before/after |
| Workflow complexity grows with limited net gain | MEDIUM | LOW | Bound scope to static-analysis decomposition only and keep deterministic lanes |

## Validation Plan

Verification contract:
- Baseline gates:
  - `pixi run build -p debug`
  - `pixi run build -p asan`
  - `pixi run build -p quality`
- Environment-agnostic automated checks:
  - `pixi run ci --lane static-analysis`
  - `pixi run ci --lane build-test`
- Environment-sensitive checks:
  - CI run inspection of PR job graph and durations in GitHub Actions
- Manual fallback:
  - allowed only for comparing CI job timing deltas when hosted-run variance prevents strict local
    equivalence
  - record run links, commit SHA, and observed job durations
- Mandatory checks with no fallback:
  - Semgrep and quality-build PR gates remain blocking
- Pass criteria:
  - overall CI workflow wall-clock improves against the baseline run for equivalent coverage
  - static-analysis PR path duration improves against the baseline run
  - local `pixi run ci` entrypoints remain deterministic and reproduce lane behavior
  - required PR coverage remains equivalent after job reshaping
