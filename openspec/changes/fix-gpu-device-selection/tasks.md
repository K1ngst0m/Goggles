## 1. CLI Option

- [ ] 1.1 Add `--gpu <selector>` option to CLI (accepts index like "0" or name substring like "AMD")
- [ ] 1.2 Pass GPU selector through to VulkanBackend

## 2. Device Selection Logic

- [x] 2.1 Log all available GPUs with index and name at startup
- [ ] 2.2 If `--gpu` specified, filter/select by the given selector
- [x] 2.3 Improve default selection: prefer discrete GPU that supports the surface
- [x] 2.4 Skip devices where `getSurfaceSupportKHR` returns false for all queue families

## 3. Child Process GPU Inheritance

- [x] 3.1 Add `gpu_uuid()` getter to VulkanBackend and Application
- [x] 3.2 Pass `GOGGLES_GPU_UUID` env var to spawned child process
- [x] 3.3 Hook `vkEnumeratePhysicalDevices` in layer to filter devices based on UUID matching

## 4. Testing

- [ ] 4.1 Test on multi-GPU system with explicit `--gpu` selection
- [x] 4.2 Test default selection picks working GPU
- [x] 4.3 Test child process uses same GPU as goggles