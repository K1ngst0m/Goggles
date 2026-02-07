## Context
Scale mode is currently cached in application state while rendering uses a copy stored in the Vulkan backend. Dynamic mode resolution requests depend on the application copy, which prevents runtime switching and risks stale requests.

## Goals / Non-Goals
- Goals:
  - Centralize scale mode ownership in the Vulkan backend.
  - Expose runtime controls via the existing Pre-Chain UI section.
  - Ensure dynamic mode resolution requests always reflect the active backend state.
- Non-Goals:
  - Changing RetroArch per-pass scale types.
  - Altering shader preset semantics or pass scale behavior.

## Decisions
- Decision: Store scale mode and integer scale in VulkanBackend as the single source of truth with explicit getters/setters.
- Decision: Route ImGui changes through Application callbacks into VulkanBackend, keeping UI state synchronized with backend accessors.
- Decision: Trigger dynamic resolution requests based on backend scale mode at swapchain resize and on dynamic-mode activation.

## Risks / Trade-offs
- Risk: UI/backend desynchronization if callbacks are not wired consistently.
  - Mitigation: Initialize UI state from backend getters and re-sync after preset reloads.

## Migration Plan
1. Add backend accessors + update API.
2. Wire ImGui Pre-Chain controls to backend setters.
3. Update Application dynamic-resolution checks to use backend getters.

## Open Questions
- Should integer scale changes be applied immediately or deferred until next frame boundary?
