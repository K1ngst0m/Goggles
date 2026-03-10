## 1. Static-analysis lane decomposition

- [x] 1.1 Extend `scripts/task/ci.sh` to expose split static-analysis lanes for Semgrep and
  quality-build execution while preserving the combined `static-analysis` lane.
- [x] 1.2 Keep stage timing output for each split lane so bottlenecks remain observable before and
  after the workflow reshape.
- [x] 1.3 Verify split lane failure semantics remain blocking for their owned checks.

## 2. CI workflow graph reshaping

- [x] 2.1 Update `.github/workflows/ci.yml` to run Semgrep and quality static-analysis lanes as
  separate PR jobs that can execute in parallel.
- [x] 2.2 Keep both split static-analysis jobs configured as required merge gates to preserve
  equivalent PR coverage.
- [x] 2.3 Preserve explicit ccache/Pixi setup per split job so job separation does not remove
  deterministic toolchain and cache behavior.

## 3. Local command surface alignment

- [x] 3.1 Update `pixi.toml` task surfaces (and any directly related CI helper docs) so local
  `pixi run ci` lane usage maps clearly to split and combined static-analysis behavior.
- [x] 3.2 Confirm canonical local reproduction remains available with `pixi run ci --lane
  static-analysis`.
- [x] 3.3 Confirm targeted local reproduction works for each split static-analysis lane.

## 4. Verification

- [x] 4.1 Run `pixi run ci --lane static-analysis` and confirm both static-analysis surfaces execute
  with blocking semantics.
- [x] 4.2 Run `pixi run ci --lane build-test` to ensure unrelated CI lane behavior is not regressed.
- [x] 4.3 Run baseline build/static gates: `pixi run build -p debug`, `pixi run build -p asan`, and
  `pixi run build -p quality`.
- [x] 4.4 Validate CI run output shows both split static-analysis PR checks, equivalent required
  coverage, and improved total/static-analysis timing versus the baseline run when measured.
