# Change: Unify external image frame metadata

## Why
The compositor and capture paths each define their own frame metadata structs, which diverge in
format representation and sequencing. This duplication makes it harder to share common image
handling logic and to add new external handle types beyond DMA-BUF.

## What Changes
- Introduce a shared `util::ExternalImage` for external image metadata + handle ownership
- Introduce a wrapper type (e.g., `ExternalImageFrame`) that pairs `ExternalImage` with
  a `frame_number` sequence
- Standardize external image format representation on `vk::Format` and convert DRM FourCC at
  ingestion boundaries

## Impact
- Affected specs: `render-pipeline`
- Affected code: compositor server frame export, capture receiver, app frame selection, render
  backend import paths
