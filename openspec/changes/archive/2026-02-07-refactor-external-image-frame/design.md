## Context
The compositor path emits DMA-BUF-backed surface frames using DRM FourCC format metadata, while
capture frames use `VkFormat`. Both paths carry similar metadata and a frame sequence number, but
are modeled as separate structs. This creates duplication and makes it unclear where conversion
should happen when adding new external handle types (e.g., shm).

## Goals / Non-Goals
- Goals:
  - Define a shared external image metadata type in `util`
  - Keep handle metadata separate from sequence information
  - Use `vk::Format` consistently for application-facing image metadata
  - Allow future handle types without renaming the struct
- Non-Goals:
  - Change IPC wire formats
  - Add new handle types now (shm support deferred)

## Decisions
- Decision: Create `util::ExternalImage` for width/height/stride/offset/format/modifier + handle.
- Decision: Add `ExternalImageFrame` wrapper containing `ExternalImage` + `frame_number`.
- Decision: Standardize `ExternalImage::format` on `vk::Format` and convert DRM FourCC at the
  compositor ingestion boundary.

## Risks / Trade-offs
- Conversion at ingestion adds a small amount of per-frame CPU work (format mapping).
- Converting DRM FourCC to `vk::Format` can yield `vk::Format::eUndefined` for unsupported
  formats; these frames will be skipped as they are today.

## Migration Plan
1. Add `util::ExternalImage` and `ExternalImageFrame` types.
2. Update compositor to convert DRM FourCC to `vk::Format` when building the frame.
3. Update capture receiver and render backend to use `ExternalImageFrame`.
4. Remove legacy `SurfaceFrame` and `CaptureFrame` structs.

## Open Questions
- None.
