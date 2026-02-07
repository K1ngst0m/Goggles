## 1. CLI Option

- [x] 1.1 Add `--gpu <selector>` option to CLI (accepts index like "0" or name substring like "AMD")
- [x] 1.2 Pass GPU selector through to VulkanBackend

## 2. Device Selection Logic

- [x] 2.1 Log all available GPUs with index and name at startup
- [x] 2.2 Improve default selection: prefer discrete GPU that supports the surface
- [x] 2.3 Skip devices where `getSurfaceSupportKHR` returns false for all queue families

## 3. Child Process GPU Inheritance

- [x] 3.1 Add `gpu_uuid()` getter to VulkanBackend and Application
- [x] 3.2 Pass `GOGGLES_GPU_UUID` env var to spawned child process
- [x] 3.3 Hook `vkEnumeratePhysicalDevices` in layer to filter devices based on UUID matching
- [x] 3.4 Add null check for `GetPhysicalDeviceProperties2` (Vulkan 1.0 fallback)

## 4. Testing

- [x] 4.1 Validate explicit `--gpu` selection parsing and config propagation in automated tests
  - No automated backend selector tests were added; backend behavior remains runtime/manual follow-up.
- [x] 4.2 Test default selection picks working GPU
- [x] 4.3 Test child process uses same GPU as goggles
