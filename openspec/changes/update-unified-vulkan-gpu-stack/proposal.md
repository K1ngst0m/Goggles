# Change: Update unified Vulkan GPU stack and compositor renderer policy

## Why
Goggles currently allows GPU selection by CLI selector and forwards UUID to the target app layer, but the compositor renderer path is still independently chosen by wlroots renderer auto-creation. This can produce split-GPU behavior (for example compositor on iGPU while viewer/layer run on dGPU), inconsistent modifier capabilities, and non-deterministic DMA-BUF interop.

The project now requires strict single-device consistency across compositor, viewer Vulkan backend, and layer-managed app capture path. We also want to remove GLES renderer usage in compositor mode and standardize on Vulkan renderer creation with explicit device mapping.

## What Changes
- Upgrade wlroots packaging target to 0.19.x and build with Vulkan renderer support enabled for Goggles compositor flow.
- Replace compositor renderer auto-creation with explicit Vulkan renderer creation using an explicit DRM render node FD.
- Introduce a unified startup policy that resolves one selected GPU UUID to a Vulkan physical device, DRM render node path, and ICD identity, then enforces that mapping across:
  - Goggles viewer Vulkan backend
  - wlroots compositor Vulkan renderer
  - spawned target app/layer environment
- Enforce hard-fail startup semantics for GPU/ICD mismatch or UUID->DRM mapping failure.
- Preserve compatibility fallback for compositor presentation buffers by allowing LINEAR DMA-BUF modifier fallback when a common non-linear modifier cannot be negotiated.

## Proposal Size Decision
This is a large cross-cutting effort, but it is best delivered as a **single proposal with phased tasks** instead of multiple loosely-coupled proposals.

Reasoning:
- GPU/ICD consistency is only meaningful when compositor + viewer + layer policies land together.
- Splitting wlroots Vulkan migration and policy enforcement into separate proposals risks shipping partial states that violate the new strict policy.
- Validation and rollback are simpler when one change-id owns all affected behavior and tests.

## Impact
- Affected specs: `app-window`, `compositor-capture`, `vk-layer-capture`, `dependency-management`
- Affected code: `src/compositor/compositor_server.cpp`, `src/compositor/compositor_server.hpp`, `src/app/application.cpp`, `src/app/main.cpp`, `src/render/backend/vulkan_backend.cpp`, `src/util/*gpu*`, `src/util/*drm*`, layer env propagation paths, `packages/wlroots_0_19*/`, `pixi.toml`, `pixi.lock`
