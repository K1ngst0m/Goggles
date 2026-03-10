## Context

The current CI static-analysis path executes Semgrep and the quality build in one serialized lane.
That composition keeps both checks authoritative but elongates pull-request feedback by making the
slowest lane a strict serial chain.

The requested outcome is to reduce both total workflow duration and static-analysis path duration
while preserving equivalent PR-gated safety and keeping `pixi run ci` as the local reproduction
surface.

This design changes workflow composition only; it does not alter runtime product behavior.

## Goals / Non-Goals

**Goals:**
- Partition static-analysis checks into independently executable lanes so CI can parallelize them.
- Keep Semgrep and quality build as blocking PR checks with equivalent merge protection.
- Preserve deterministic local reproduction through `pixi run ci`.
- Keep CI contracts explicit in OpenSpec artifacts and implementation-context mapping.

**Non-Goals:**
- Changing what Semgrep checks for or weakening quality-build semantics.
- Moving required checks outside PR-triggered gating.
- Reworking unrelated CI lanes (`format`, `build-test`) beyond integration touch points.
- Defining a strict numeric SLA for job durations.

## Decisions

### Decision: Split static-analysis into two canonical lanes

The CI task backend SHALL expose separate static-analysis lanes for:
- Semgrep + fixture verification
- quality-build execution

Rationale:
- The current serial lane composes independent checks that can be run concurrently.
- Lane decomposition keeps command ownership in one repository-controlled script while enabling CI
  job-level scheduling improvements.

Alternatives considered:
- Keep one static-analysis lane and tune compiler flags only: rejected because it cannot unlock
  workflow-level parallelism.
- Move Semgrep to non-blocking informational checks: rejected because it violates the required
  blocking-gate contract.

### Decision: Parallelize PR job graph while preserving equivalent required coverage

The GitHub Actions workflow SHALL run Semgrep and quality-build static-analysis lanes in separate
PR jobs that are both required before merge.

Rationale:
- Parallel jobs reduce critical path time without dropping checks.
- Separate job visibility improves diagnosis when one static-analysis surface regresses.

Alternatives considered:
- Keep one PR job with background subprocess fan-out: rejected because job-level result handling and
  observability are weaker than explicit parallel jobs.
- Shift one check to post-merge/nightly: rejected because it reduces PR-time safety.

### Decision: Preserve a combined local lane as deterministic reproduction contract

`pixi run ci --lane static-analysis` SHALL remain a combined local entrypoint that runs both
static-analysis surfaces in a deterministic repository-defined order, while exposing split lanes for
targeted reproduction.

Rationale:
- Contributors rely on one canonical local command that reflects CI intent.
- Split lanes are still needed for focused debugging and CI wiring.

Alternatives considered:
- Remove combined lane and require two manual commands locally: rejected because it increases drift
  risk and operator burden.

## Risks / Trade-offs

- [Coverage drift during workflow refactor] -> enforce both split jobs as required PR checks and keep
  spec scenarios explicit.
- [Local/CI contract mismatch] -> keep split and combined behavior codified in one task backend and
  one spec delta.
- [Marginal speed gain despite decomposition] -> retain timing-stage instrumentation and compare
  baseline vs updated run data.
- [Cache invalidation from job separation] -> preserve explicit ccache setup and restore keys for
  each split job.

## Migration Plan

1. Update CI OpenSpec requirement deltas to codify split-lane + equivalent-gate behavior.
2. Extend `scripts/task/ci.sh` lane model to provide separate Semgrep and quality lanes while
   keeping combined static-analysis behavior.
3. Update `pixi.toml`/workflow command surfaces so local and CI use canonical lane entrypoints.
4. Reshape `.github/workflows/ci.yml` static-analysis graph into parallel required jobs.
5. Verify baseline gates, lane commands, and CI timing/coverage outcomes.

Rollback strategy:
- Revert lane split and workflow graph changes together to restore prior serialized behavior if
  coverage or determinism contracts break.

## Open Questions

- None.
