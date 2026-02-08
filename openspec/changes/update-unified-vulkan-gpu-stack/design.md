## Context

Current runtime behavior can diverge across three GPU decision points:
- Viewer Vulkan backend selects a physical device using selector/index/name logic and exports UUID.
- Layer side receives `GOGGLES_GPU_UUID` and filters application-visible devices.
- Compositor currently calls wlroots renderer auto-create and may pick a different render node/device.

This violates single-GPU policy expectations and can degrade DMA-BUF compatibility. It also makes diagnosis difficult because each subsystem can silently choose a different node or driver stack.

## Goals / Non-Goals

**Goals:**
- Force compositor to use Vulkan renderer only (no GLES fallback path in Goggles compositor mode).
- Resolve one canonical runtime GPU identity from selected UUID and enforce it in all components.
- Enforce a single ICD identity across compositor/viewer/layer-spawned app process.
- Hard-fail on mismatch.
- Keep modifier compatibility by allowing LINEAR fallback for compositor presentation buffers.

**Non-Goals:**
- Implementing a custom wlroots renderer backend.
- Supporting best-effort fallback to a different GPU when strict mapping fails.
- Replacing existing layer capture transport/protocol semantics beyond policy checks.

## Key Decisions

- Decision: Use wlroots Vulkan renderer via explicit creation (`wlr_vk_renderer_create_with_drm_fd`) instead of `wlr_renderer_autocreate`.
  - Rationale: removes renderer backend ambiguity and prevents implicit GLES selection.

- Decision: Keep this as one proposal/change-id with phased implementation.
  - Rationale: consistency policy is atomic across subsystems; partial rollout is unsafe.

- Decision: Resolve `UUID -> DRM render node` in Goggles and pass the resolved DRM FD/path into compositor setup.
  - Rationale: compositor must be pinned to the same physical GPU chosen by viewer policy.

- Decision: Enforce ICD identity by propagating the same ICD env configuration to all child processes and validating startup assumptions.
  - Rationale: matching UUID alone is insufficient when multiple ICDs/loaders are present.

- Decision: Hard-fail on any policy violation.
  - Rationale: strict consistency was explicitly requested; silent fallback hides correctness issues.

- Decision: Preserve LINEAR modifier fallback in compositor present path.
  - Rationale: ensures cross-device/driver interop safety where optimal tiled modifiers are not mutually importable.

## Architecture Outline

1. **Canonical GPU policy object**
   - Fields: selected UUID, selected Vulkan physical device identifier, resolved DRM render node path, resolved ICD identity, strict mode enabled.
   - Constructed once during startup after CLI/config resolution.

2. **UUID to DRM mapping**
   - Enumerate candidate Vulkan devices and gather UUID + DRM mapping metadata.
   - Resolve render node path from DRM properties/libdrm.
   - Validate node access/open.

3. **Compositor renderer initialization**
   - Open resolved render node FD.
   - Create wlroots Vulkan renderer explicitly with that FD.
   - Reject startup if renderer is not Vulkan or creation fails.

4. **ICD consistency enforcement**
   - Normalize and pin ICD environment values for parent process and spawned app process.
   - Ensure layer path inherits same ICD env (plus existing `GOGGLES_GPU_UUID`).
   - Add startup logging of selected UUID + render node + ICD identity at subsystem boundaries.

5. **Modifier negotiation behavior**
   - Attempt preferred non-linear modifiers where available and mutually supported.
   - Fallback to LINEAR if negotiation/import capability fails.

## Risks / Trade-offs

- wlroots Vulkan renderer remains flagged experimental upstream in 0.18/0.19.
  - Mitigation: strict startup checks, explicit failure messaging, robust smoke/regression test matrix.

- Strict hard-fail policy can block startup on systems with broken/missing DRM metadata.
  - Mitigation: clear diagnostics and explicit remediation hints.

- ICD normalization can vary by environment manager/container setup.
  - Mitigation: centralize env construction and document precedence.

## Verification Strategy

- Unit tests for UUID->DRM mapping and policy resolution.
- Integration tests for compositor Vulkan-only init path (positive and negative).
- Manual matrix for multi-GPU hosts (iGPU+dGPU):
  - valid UUID + valid ICD
  - valid UUID + mismatched ICD
  - missing render node
  - modifier negotiation requiring LINEAR fallback

## Rollback Strategy

- Keep change isolated behind one proposal change-id.
- If Vulkan compositor stability is unacceptable, revert this change-id as a whole and return to archived behavior until follow-up proposal.
