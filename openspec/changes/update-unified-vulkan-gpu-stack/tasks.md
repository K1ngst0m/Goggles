## 1. Dependency and Packaging Baseline

- [x] 1.1 Add/update a wlroots 0.19.x package recipe and wire it into Pixi dependency resolution.
- [x] 1.2 Ensure wlroots build config enables Vulkan renderer support required by Goggles compositor mode.
- [x] 1.3 Update `pixi.toml` and lockfile to consume the new wlroots package version in default dev/test workflows.

## 2. Unified GPU Policy Core

- [ ] 2.1 Introduce a canonical runtime GPU policy structure containing selected UUID, resolved DRM render node, and ICD identity.
- [ ] 2.2 Implement UUID->DRM render node resolution in util/backend startup code and return rich `Result<T>` errors on failure.
- [ ] 2.3 Add strict mismatch detection and hard-fail semantics for unresolved UUID mapping, missing node access, or ICD mismatch.

## 3. Compositor Vulkan Renderer Enforcement

- [ ] 3.1 Replace wlroots renderer auto-create usage with explicit Vulkan renderer creation using resolved DRM FD.
- [ ] 3.2 Remove compositor GLES fallback behavior in Goggles compositor mode; initialization must fail if Vulkan renderer cannot be created.
- [ ] 3.3 Add compositor startup logs for renderer backend type, selected UUID, render node, and Vulkan physical device identity.

## 4. Layer/App Process Consistency

- [ ] 4.1 Propagate normalized ICD env and selected UUID to spawned target app process together with existing capture env.
- [ ] 4.2 Ensure layer-side GPU filtering and initialization preserve the same UUID/ICD policy and fail safely on mismatch.
- [ ] 4.3 Add startup validation checks proving viewer, compositor, and layer/app share one canonical GPU policy.

## 5. Modifier Negotiation and Fallback

- [ ] 5.1 Keep preferred modifier negotiation for compositor presentation buffers when supported.
- [ ] 5.2 Preserve LINEAR fallback behavior when no common non-linear modifier can be used safely.
- [ ] 5.3 Add logging that reports selected modifier path (preferred vs LINEAR fallback) once per swapchain reconfiguration.

## 6. Verification

- [ ] 6.1 Automated: `pixi run build -p debug && pixi run build -p test && pixi run test -p test`.
- [ ] 6.2 Automated: targeted tests for GPU policy resolution and UUID->DRM mapping logic.
- [ ] 6.3 Manual: multi-GPU run confirms compositor/viewer/layer are pinned to same UUID and same ICD.
- [ ] 6.4 Manual: invalid UUID, missing render node, and ICD mismatch each produce deterministic hard-fail startup errors.
- [ ] 6.5 Manual: compositor present path demonstrates modifier fallback to LINEAR when preferred modifier import fails.
