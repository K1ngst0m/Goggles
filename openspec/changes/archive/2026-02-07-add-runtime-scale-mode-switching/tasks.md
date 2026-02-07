## 1. Backend Ownership
- [x] 1.1 Add VulkanBackend accessors for scale_mode/integer_scale and a runtime update API
- [x] 1.2 Ensure render path uses backend state as the single source of truth
- [x] 1.3 Update Application dynamic-resolution logic to query backend scale mode

## 2. ImGui Controls
- [x] 2.1 Extend ImGuiLayer state and callbacks for scale mode selection in Pre-Chain controls
- [x] 2.2 Add Pre-Chain UI for scale mode selection and integer scale (when integer mode is active)
- [x] 2.3 Sync ImGui state from backend accessors on startup and when changes occur

## 3. Behavior Validation
- [x] 3.1 Verify dynamic mode sends resolution requests on activation and swapchain resize
  - Verified via runtime code path and test build validation (`pixi run build -p test`, `pixi run test -p test`).
- [x] 3.2 Verify scale mode changes apply to subsequent frames without restart
  - Verified via runtime code path and test build validation (`pixi run build -p test`, `pixi run test -p test`).
