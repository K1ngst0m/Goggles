# Verification Report: extract-filter-chain release pin

## status

passed

## executive_summary

The standalone `goggles-filter-chain` release tag `v0.1.0` now points to the intended final commit `6e8d68922da5fca9c379b85debae3fe390ebea89`, and `goggles/filter-chain` is repinned to that tagged release state.

Final release verification passed in both repositories: the standalone repo passed its release-state verification lanes, and the Goggles monorepo passed the required full CI gate with the release-pinned submodule.

## completed_scope

- Remote standalone tag `v0.1.0` moved from `5f14bae36baf5364e5884d40ba43e4ae4a840fc5` to `6e8d68922da5fca9c379b85debae3fe390ebea89`.
- Goggles submodule `filter-chain/` updated to detached `v0.1.0` at `6e8d68922da5fca9c379b85debae3fe390ebea89`.
- Final standalone and Goggles verification rerun on the release-pinned state.
- `openspec/changes/extract-filter-chain/tasks.md` updated to mark `T4.1` and `T4.3` complete.

## verification_results

### Standalone repo: `/home/kingstom/workspaces/goggles-filter-chain`

- `pixi install` -> passed
- `pixi run format-check` -> passed
- `pixi run build -p asan` -> passed
- `ctest --preset asan --output-on-failure` -> passed
  - Included `fc_contract_tests`, `fc_test_temporal_golden`, and `fc_test_semantic_probes`
  - Expected fixture skips remained limited to missing optional MBZ/internal preset content
- `pixi run consumer-validation -p test` -> passed
  - C API consumer passed
  - Static C++ consumer passed
  - Shared consumer reported skipped because the install tree for this verification was static-only
- `pixi run static-analysis` -> passed

### Goggles monorepo: `/home/kingstom/workspaces/goggles`

- `pixi run ci --runner container --cache-mode warm --lane all` -> passed
- Build/test, format, semgrep, and quality lanes all completed successfully against `filter-chain` pinned to `v0.1.0`

## release_pin_evidence

- `git -C /home/kingstom/workspaces/goggles-filter-chain ls-remote --tags origin v0.1.0` -> `6e8d68922da5fca9c379b85debae3fe390ebea89 refs/tags/v0.1.0`
- `git -C /home/kingstom/workspaces/goggles-filter-chain rev-parse v0.1.0^{commit}` -> `6e8d68922da5fca9c379b85debae3fe390ebea89`
- `git -C /home/kingstom/workspaces/goggles submodule status` -> `+6e8d68922da5fca9c379b85debae3fe390ebea89 filter-chain (v0.1.0)`

## risks

- The Goggles worktree still contains many pre-existing uncommitted extraction changes outside this release-pin closure; this verification proves the current worktree passes, but the pin is not yet recorded in a commit.
- Standalone consumer validation still skips the shared-consumer leg when only the static install artifact is present; that is the current scripted behavior observed during release verification.

## t2_7_refresh_2026_03_16

- Re-ran the standalone T2.7 validation gate from `/home/kingstom/workspaces/goggles-filter-chain` using the authoritative task commands: `pixi install`, `cmake --preset test`, `cmake --build --preset test`, `ctest --preset test --output-on-failure`, `bash scripts/validate-installed-consumers.sh --preset test`, `cmake --preset quality`, and `cmake --build --preset quality`.
- The gate passed end-to-end.
- The standalone test run covered the FC-owned verification split now required by the change, including `fc_contract_tests`, `fc_test_temporal_golden`, and `fc_test_semantic_probes` in the standalone repository rather than Goggles host CI.
- Consumer validation completed successfully via `scripts/validate-installed-consumers.sh --preset test`.
- `openspec/changes/extract-filter-chain/tasks.md` was updated to mark `T2.7` complete.
