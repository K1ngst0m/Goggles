## 1. Proposal Refinement

- [x] 1.1 Confirm install paths (`XDG_DATA_HOME` fallback behavior)
- [x] 1.2 Decide versioning strategy for installed layer libs (versioned vs in-place)
- [x] 1.3 Decide uninstall UX (command vs docs-only)

## 2. Build + Staging

- [x] 2.1 Define a Pixi task to build release viewer + both layer arches
- [x] 2.2 Add a staging step that collects artifacts into an AppDir layout
- [x] 2.3 Ensure staged manifests reference installed user paths (not build paths)

## 3. AppImage Entrypoint (Wrapper)

- [x] 3.1 Implement idempotent self-install with atomic writes (temp + rename)
- [x] 3.2 Install manifests into `${XDG_DATA_HOME:-$HOME/.local/share}/vulkan/implicit_layer.d/`
- [x] 3.3 Install layer libs into `${XDG_DATA_HOME:-$HOME/.local/share}/goggles/vulkan-layers/<version>/...`
- [x] 3.4 Add a `--self-install-only` mode for debugging
- [x] 3.5 Add an optional uninstall path if chosen in 1.3

## 4. Steam/Proton Compatibility

- [x] 4.1 Ensure `goggles -- %command%` launch flow sets `GOGGLES_CAPTURE=1` for the spawned command
- [x] 4.2 Ensure 32-bit layer is installed and discoverable for Proton games
- [x] 4.3 Document known Steam Runtime caveats (container visibility, permissions)

## 5. Documentation

- [x] 5.1 Update `README.md` with AppImage install/run instructions
- [x] 5.2 Add a dedicated doc page for Steam setup + troubleshooting

## 6. Validation

- [x] 6.1 Check: Vulkan loader (64-bit) discovers `VK_LAYER_goggles_capture_64`
- [x] 6.2 Check: i386 manifest + library install paths are created and consistent
- [x] 6.3 Check: Steam/Deck guidance documented for external validation
- [x] 6.4 Add a lightweight diagnostic command to print detected install state

## 7. Assets + Shader Packs

- [x] 7.1 Ensure packaged runtime uses a stable shader base directory (not CWD-dependent)
- [x] 7.2 Add a non-Pixi shader pack fetch/update flow for AppImage users
- [x] 7.3 Document where assets/shaders live (AppImage vs XDG)
- [x] 7.4 Ensure ImGui preset catalog scans XDG/AppImage shader roots (not CWD-dependent)
- [x] 7.5 Fix AppRun shader-pack detection output (`--print-install-state`)
