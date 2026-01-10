# Change: Add Dynamic Resolution Request

## Why

When the Goggles window aspect ratio doesn't match the source, users can only choose between stretching, cropping, or letterboxing. Dynamic resolution requests allow Goggles to notify the source application to adjust its rendering resolution, achieving lossless aspect ratio synchronization.

## What Changes

- Extend `CaptureControl` protocol with resolution request fields
- Add `request_resolution()` method to `CaptureReceiver`
- Handle resolution requests in vk_layer (WSI Proxy mode only)
- Send resolution requests on window resize in application

## Impact

- Affected specs: `vk-layer-capture`
- Affected code:
  - `src/capture/capture_protocol.hpp` - protocol extension
  - `src/capture/capture_receiver.cpp/hpp` - send requests
  - `src/capture/vk_layer/ipc_socket.cpp` - receive requests
  - `src/capture/vk_layer/wsi_virtual.cpp/hpp` - resolution update
  - `src/app/application.cpp` - resize trigger