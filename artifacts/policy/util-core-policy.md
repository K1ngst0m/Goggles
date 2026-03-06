# util-core Ownership and Version Policy (`util-core-v1`)

## Ownership Authority

- Owner: `src/util` maintainers (`goggles-core-maintainers`)
- Change control: policy updates require explicit review before release gating is green.

## Version Policy

- Scheme: Semantic Versioning.
- Compatibility target: ABI v1 host/runtime contracts.
- Breaking util-core API changes require:
  - major version increment,
  - updated compatibility gates,
  - host and standalone runtime intake validation.

## Runtime-Allowed util-core Surface

- `util/config.hpp`
- `util/error.hpp`
- `util/logging.hpp`
- `util/profiling.hpp`
- `util/serializer.hpp`

## Host-Only Utility Surface (Excluded from Standalone Runtime)

- `util/external_image.hpp`
- `util/job_system.hpp`
- `util/unique_fd.hpp`
- `util/paths.hpp`
- `util/queues.hpp`
- `util/drm_format.hpp`
- `util/drm_fourcc.hpp`

