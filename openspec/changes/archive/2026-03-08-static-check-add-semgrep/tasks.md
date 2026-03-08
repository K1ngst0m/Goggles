## 1. Spec & Design

- [x] 1.1 Add `ci` delta requirements for a repo-controlled blocking Semgrep gate in the existing static-analysis workflow.
- [x] 1.2 Add `build-system` delta requirements for Pixi-managed deterministic Semgrep tooling and path-scoped rule sources.
- [x] 1.3 Keep `design.md` aligned with the narrow Semgrep boundary: checked-in rules only, approved high-signal bans only, and no duplication of stronger existing tooling.

## 2. Workflow & Configuration

- [x] 2.1 Add a checked-in Semgrep configuration and local rule set for the approved policy-derived bans.
- [x] 2.2 Update `pixi.toml` and `pixi.lock` together to provide `pixi run semgrep` for local and CI use.
- [x] 2.3 Update `.github/workflows/ci.yml` so the `static-analysis` job runs the Semgrep gate without removing `pixi run build -p quality`.
- [x] 2.4 Limit the initial Semgrep scan to `src/` and `tests/`, then scope subsystem-sensitive rules to the correct directories and exclude known policy exceptions such as `src/capture/vk_layer/`.

## 3. Dependency Governance

- [x] 3.1 Record Semgrep dependency rationale, license compatibility review, maintenance assessment, and team agreement in the implementation or PR evidence.
- [x] 3.2 Verify the selected Semgrep package is admitted through Pixi source-of-truth files rather than an environment-local install path.

## 4. Verification

- [x] 4.1 Run `openspec validate static-check-add-semgrep --strict`.
- [x] 4.2 Run `pixi run bash -c 'command -v semgrep && semgrep --version'` and confirm the resolved binary comes from the Pixi-managed environment.
- [x] 4.3 Add tracked positive and negative Semgrep verification inputs for each claimed policy clause.
- [x] 4.4 Verify the positive inputs cover representative repo-style patterns for the targeted code shapes, not only toy single-line snippets.
- [x] 4.5 Verify scoped rules cover the directories they claim to govern and do not flag documented exception paths.
- [x] 4.6 Run `pixi run semgrep` locally and confirm checked-in rules catch the positive inputs, allow the negative inputs, and use checked-in rules only.
- [x] 4.7 Run `pixi run build -p quality` and confirm the Semgrep addition does not replace the existing quality gate.
- [x] 4.8 Record CI evidence showing the same Semgrep version surface and path provenance before the blocking scan runs.
- [x] 4.9 Record any no-fallback static-check expectations and rule-coverage evidence in the implementation notes or PR description when the change is applied.
