## Context
The repository already has a `static-analysis` CI job in `.github/workflows/ci.yml` that runs `pixi run build -p quality`. There is no Semgrep dependency, no checked-in Semgrep config, and no local task that guarantees the same static-pattern checks run in both developer workflows and CI.

The proposal constrains Semgrep tightly: it must be a blocking gate, it must be repo-controlled, and it must stay limited to policy clauses from `docs/project_policies.md` that are realistically Semgrep-enforceable. Formatting, naming, include ordering, many clang-tidy-style semantic checks, preset/lockfile checks, and runtime validation remain with the tools that already own them.

## Goals / Non-Goals

**Goals:**
- Add the deterministic `pixi run semgrep` entrypoint that local developers and CI both use.
- Keep all Semgrep configuration and rules checked into the repository.
- Encode only narrow, high-signal policy bans that Semgrep can enforce reliably.
- Prove each claimed Semgrep-enforced policy clause with tracked positive and negative verification inputs.
- Preserve the existing `pixi run build -p quality` gate instead of collapsing multiple tool responsibilities into one scanner.

**Non-Goals:**
- No Semgrep registry or hosted rule dependencies.
- No broad “scan everything” policy sweep.
- No replacement of clang-tidy, formatting checks, lockfile checks, or runtime validation.
- No policy rewrite in this change.

## Decisions
- Decision: Use a checked-in Semgrep config and checked-in local rule directory.
  - The change will keep Semgrep configuration under repository control so local and CI behavior derive from the same committed inputs.
  - Rationale: this satisfies the repo-controlled constraint and keeps proposal/apply verification observable.
  - Alternative considered: use Semgrep registry defaults or a hosted policy.
  - Rejected: that would make rule contents drift outside the repository and break deterministic review.

- Decision: Run Semgrep through a Pixi-managed entrypoint and keep it inside the existing static-analysis lane.
  - CI will invoke `pixi run semgrep`, the same repository-defined command that developers run locally, and the existing `pixi run build -p quality` step remains part of the lane.
  - Rationale: one shared entrypoint reduces environment drift, while keeping clang-tidy coverage intact.
  - Alternative considered: run Semgrep via a standalone GitHub Action or ad hoc binary install.
  - Rejected: that would weaken determinism and duplicate environment management outside Pixi.

- Decision: Admit Semgrep through Pixi source-of-truth files and record dependency-governance checks in the change.
  - The apply work MUST update `pixi.toml` and `pixi.lock` together, and the change artifacts MUST make Semgrep rationale, license compatibility review, maintenance assessment, and team agreement explicit.
  - Rationale: `docs/project_policies.md` requires Pixi to remain the dependency source of truth and requires new dependencies to carry admission evidence.
  - Alternative considered: rely on transitive or environment-local Semgrep installation without lockfile or review metadata.
  - Rejected: that would violate deterministic environment policy and make dependency admission non-reviewable.

- Decision: Keep the initial Semgrep ruleset narrow and policy-derived.
  - Initial rule coverage should focus on the approved high-signal bans from `docs/project_policies.md`: banned subsystem logging APIs, `using namespace` in headers, raw `new`/`delete`, raw `Vk*` in app code, `vk::Unique*`/`vk::raii::*` in app code, `static_cast<void>(...)` on Vulkan result-returning calls, and direct `std::thread`/`std::jthread` in render or pipeline paths.
  - The initial scan surface should be repository-managed C/C++ sources under `src/` and `tests/`, with per-rule path filters for narrower policy scopes.
  - Claimed coverage must be backed by tracked verification inputs that exercise representative repo-style matches, non-matches, and documented exception paths.
  - Rationale: these patterns are specific enough to gate automatically without trying to replace review judgment.
  - Alternative considered: add every clause that might be technically toolable.
  - Rejected: broad scope would increase noise, duplicate stronger tooling, and make ownership of failures unclear.

- Decision: Path-scope subsystem-sensitive rules instead of pretending policy is uniform everywhere.
  - Rules that depend on policy scope, such as raw `Vk*` bans or render-path threading bans, must target only the directories where the policy applies and exclude known exceptions such as `src/capture/vk_layer/`.
  - Rationale: subsystem-specific policy is already part of the repository contract and must remain explicit in the static checks.
- Alternative considered: apply the same rule to all C/C++ code.
  - Rejected: that would create false positives against allowed subsystem-specific usage.

## Verification contract
- Baseline gates:
  - `pixi run semgrep`
  - `pixi run build -p quality`
- Environment-agnostic automated checks:
  - `openspec validate static-check-add-semgrep --strict`
  - `pixi run bash -c 'command -v semgrep && semgrep --version'`
- Required rule-verification inputs:
  - tracked positive and negative verification inputs for each claimed policy clause
  - representative repo-style patterns for targeted code shapes, not only toy single-line snippets
  - scoped verification inputs covering intended directories and documented exception paths
- Environment-sensitive checks:
  - None.
- Manual fallback:
  - None for the static Semgrep gate.
- Mandatory checks with no fallback:
  - `pixi run semgrep`
  - `pixi run build -p quality`
- Pass criteria:
  - `pixi run bash -c 'command -v semgrep && semgrep --version'` reports a Semgrep binary from the Pixi-managed environment and a version surface that matches the locked dependency.
  - `pixi run semgrep` exits successfully using only checked-in Semgrep configuration and rules.
  - Each claimed policy clause is proven by tracked positive inputs that fail when the rule is active and tracked negative inputs that remain clean.
  - Positive verification inputs cover representative repo-style code shapes for the claimed policy clause.
  - CI records the Semgrep path provenance and version surface before running the blocking scan.
  - The static-analysis lane still includes `pixi run build -p quality` after Semgrep is added.
  - Scoped rules do not apply to directories outside their policy boundary.

## Risks / Trade-offs
- False-positive drift from path mistakes or overly generic patterns -> mitigate by keeping the initial rule set small and path-scoped.
- Added CI cost in the static-analysis lane -> mitigate by reusing the existing job and keeping Semgrep limited to targeted code roots.
- Semgrep may still miss semantic policy violations that humans catch -> accept this boundary explicitly and keep review-only rules out of scope.

## Migration Plan
1. Add proposal artifacts for `ci` and `build-system` behavior changes.
2. Add repo-controlled Semgrep dependency/configuration through `pixi.toml` and `pixi.lock`, and document dependency-admission evidence.
3. Wire the existing static-analysis lane to run the Semgrep gate alongside the current quality build.
4. Add tracked verification inputs that prove the checked-in rules catch the claimed policy violations, allow approved counterexamples, and respect documented scope exceptions.
5. Verify the checked-in rules only cover the approved high-signal policy bans and do not replace stronger existing tooling.

## Open Questions
- None blocking. Exact rule file names and final path globs can be decided during apply as long as they preserve the scoped rule categories and deterministic entrypoint contract.
