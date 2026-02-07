## 1. Implementation
- [x] 1.1 Add `util::ExternalImage` and `ExternalImageFrame` types
- [x] 1.2 Convert compositor DRM FourCC to `vk::Format` at frame creation
- [x] 1.3 Update capture receiver to use `ExternalImageFrame`
- [x] 1.4 Update app frame selection logic to use unified types
- [x] 1.5 Update render backend imports to use unified types
- [x] 1.6 Remove `SurfaceFrame` and `CaptureFrame` definitions

## 2. Validation
- [x] 2.1 Build with a debug preset
- [x] 2.2 Run existing unit tests (if applicable)
