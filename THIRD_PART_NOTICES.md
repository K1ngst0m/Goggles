# Third-Party Notices

This file documents third-party code and assets that are included in this repository.
Third-party components keep their own license terms.

## Included Notices

- `assets/fonts/OFL.txt`
  - Component: Bundled font assets used by Goggles.
  - License: SIL Open Font License 1.1 (OFL-1.1).
  - Note: Full license text is included in the file.

- `shaders/retroarch/crt/shaders/crt-lottes-fast.slang`
  - Component: Curated RetroArch CRT shader (Timothy Lottes, adapted by hunterk).
  - License: Unlicense (public domain dedication).
  - Note: License block is embedded in the shader source.

- `shaders/retroarch/crt/crt-lottes-fast.slangp`
  - Component: Preset metadata for the curated shader above.
  - License: Distributed with and attributed to upstream `libretro/slang-shaders` content.

- `src/compositor/protocol/xdg-shell-protocol.h`
  - Component: Generated Wayland protocol header.
  - License: Permissive MIT-style license (embedded in file header).

- `src/compositor/protocol/pointer-constraints-unstable-v1-protocol.h`
  - Component: Generated Wayland protocol header.
  - License: Permissive MIT-style license (embedded in file header).

- `src/compositor/protocol/relative-pointer-unstable-v1-protocol.h`
  - Component: Generated Wayland protocol header.
  - License: Permissive MIT-style license (embedded in file header).

- `src/compositor/protocol/wlr-layer-shell-unstable-v1-protocol.h`
  - Component: Generated Wayland protocol header.
  - License: ISC-style permissive license (embedded in file header).

- `cmake/CPM.cmake`
  - Component: CPM.cmake dependency manager helper.
  - License: MIT.
  - Note: License notice is embedded in the file header.

## Dependency Note

Runtime/build dependencies resolved through Pixi/Conda and other package sources are licensed
under their own terms. See `pixi.lock` and package metadata for details.
