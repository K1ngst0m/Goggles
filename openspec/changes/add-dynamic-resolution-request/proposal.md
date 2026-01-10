# Change: Add Dynamic Resolution Request

## Why

When the Goggles window aspect ratio doesn't match the source, users can only choose between stretching, cropping, or letterboxing. Dynamic resolution requests allow Goggles to notify the source application to adjust its rendering resolution, achieving lossless aspect ratio synchronization.

## What Changes

- Add `dynamic` scale mode that triggers resolution requests on window resize
- **BREAKING**: Extend `CaptureControl` protocol with resolution request fields (changes struct layout from `capturing + reserved[]` to `flags + requested_width + requested_height`, breaking binary compatibility with existing viewers/layers)
- Add `request_resolution()` method to `CaptureReceiver`
- Handle resolution requests in vk_layer (WSI Proxy mode only)

## Impact

- Affected specs: `vk-layer-capture`, `render-pipeline`
- Affected code:
  - `src/util/config.hpp/cpp` - add dynamic scale mode
  - `src/capture/capture_protocol.hpp` - protocol extension
  - `src/capture/capture_receiver.cpp/hpp` - send requests
  - `src/capture/vk_layer/ipc_socket.cpp` - receive requests
  - `src/capture/vk_layer/wsi_virtual.cpp/hpp` - resolution update
  - `src/app/application.cpp` - resize trigger