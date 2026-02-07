# Change: Refactor compositor module and public API

## Why
The compositor server now handles both input forwarding and compositor-presented surface frames. The current InputForwarder wrapper adds indirection without clear separation of concerns. Aligning module structure and public API with current responsibilities reduces confusion and simplifies integration.

## What Changes
- Move compositor implementation into a dedicated `src/compositor/` module.
- Remove `InputForwarder`; expose SDL input forwarding methods directly on `CompositorServer`.
- Update application wiring and build targets to depend on the compositor module.
- Consolidate compositor entry points under a single public header (`compositor_server.hpp`).
- Relocate generated Wayland protocol headers under `src/compositor/protocol/`.

## Impact
- Affected specs: `input-forwarding`
- Affected code: `src/input/*`, `src/app/*`, CMake targets for input/compositor
