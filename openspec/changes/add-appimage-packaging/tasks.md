## 1. Proposal Refinement

- [ ] 1.1 Confirm install paths (`XDG_DATA_HOME` fallback behavior)
- [ ] 1.2 Decide versioning strategy for installed layer libs (versioned vs in-place)
- [ ] 1.3 Decide uninstall UX (command vs docs-only)

## 2. Build + Staging

- [ ] 2.1 Define a Pixi task to build release viewer + both layer arches
- [ ] 2.2 Add a staging step that collects artifacts into an AppDir layout
- [ ] 2.3 Ensure staged manifests reference installed user paths (not build paths)

## 3. AppImage Entrypoint (Wrapper)

- [ ] 3.1 Implement idempotent self-install with atomic writes (temp + rename)
- [ ] 3.2 Install manifests into `${XDG_DATA_HOME:-$HOME/.local/share}/vulkan/implicit_layer.d/`
- [ ] 3.3 Install layer libs into `${XDG_DATA_HOME:-$HOME/.local/share}/goggles/vulkan-layers/<version>/...`
- [ ] 3.4 Add a `--self-install-only` mode for debugging
- [ ] 3.5 Add an optional uninstall path if chosen in 1.3

## 4. Steam/Proton Compatibility

- [ ] 4.1 Ensure `goggles -- %command%` launch flow sets `GOGGLES_CAPTURE=1` for the spawned command
- [ ] 4.2 Ensure 32-bit layer is installed and discoverable for Proton games
- [ ] 4.3 Document known Steam Runtime caveats (container visibility, permissions)

## 5. Documentation

- [ ] 5.1 Update `README.md` with AppImage install/run instructions
- [ ] 5.2 Add a dedicated doc page for Steam setup + troubleshooting

## 6. Validation

- [ ] 6.1 Manual test: native Vulkan app (64-bit) loads layer via implicit manifest
- [ ] 6.2 Manual test: Steam game (Proton) loads 32-bit layer (verify via logs)
- [ ] 6.3 Manual test: Steam Deck path (desktop/gaming mode as feasible)
- [ ] 6.4 Add a lightweight diagnostic command to print detected install state

