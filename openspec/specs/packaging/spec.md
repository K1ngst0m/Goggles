# packaging Specification

## Purpose
TBD - created by archiving change add-appimage-packaging. Update Purpose after archive.
## Requirements
### Requirement: AppImage Distribution

The project SHALL provide an AppImage distribution artifact for the Goggles viewer that runs on arbitrary Linux distributions without requiring root installation.

#### Scenario: AppImage starts viewer
- **GIVEN** the user has downloaded the Goggles AppImage
- **WHEN** the user executes the AppImage
- **THEN** the Goggles viewer SHALL start successfully

### Requirement: User-Level Vulkan Layer Self-Install

The AppImage entrypoint SHALL support a one-time, user-level installation of Vulkan implicit layer manifests and layer shared libraries for both 64-bit and 32-bit architectures.

The installation SHALL target:
- `${XDG_DATA_HOME:-$HOME/.local/share}/vulkan/implicit_layer.d/` for manifests
- `${XDG_DATA_HOME:-$HOME/.local/share}/goggles/vulkan-layers/<version>/...` for layer shared libraries

#### Scenario: First run installs manifests and libraries
- **GIVEN** no Goggles layer manifests exist in the user implicit layer directory
- **WHEN** the user runs the AppImage
- **THEN** the manifests for i386 and x86_64 SHALL be installed into the implicit layer directory
- **AND** the corresponding layer libraries SHALL be installed into the Goggles user data directory

#### Scenario: Subsequent runs are idempotent
- **GIVEN** the manifests and layer libraries are already installed for the current version
- **WHEN** the user runs the AppImage again
- **THEN** the install step SHALL NOT corrupt or partially rewrite installed files
- **AND** the viewer launch SHALL proceed normally

### Requirement: Steam-Safe Layer Activation

The packaged launch flow SHALL NOT rely on `VK_LAYER_PATH` or `VK_ADD_LAYER_PATH` to activate the capture layer.

Instead, layer activation SHALL depend on:
- implicit layer discovery via installed manifests
- the existing enable environment variable `GOGGLES_CAPTURE=1`

#### Scenario: Steam runtime sanitizes Vulkan layer env vars
- **GIVEN** a Steam runtime environment that strips `VK_LAYER_PATH` / `VK_ADD_LAYER_PATH`
- **WHEN** a game is launched through Goggles with `GOGGLES_CAPTURE=1`
- **THEN** the Vulkan loader SHALL still discover the Goggles layer via the implicit manifest search path
- **AND** the layer SHALL load successfully

### Requirement: Steam Launch UX Compatibility

The packaging SHALL support Steam launch options of the form `goggles -- %command%`.

#### Scenario: Launch option passthrough
- **GIVEN** Steam is configured with launch options `goggles -- %command%`
- **WHEN** Steam launches the game
- **THEN** Goggles SHALL execute the target command exactly as provided by Steam
- **AND** set `GOGGLES_CAPTURE=1` for the spawned game process

### Requirement: Packaged Assets Are Not CWD-Dependent

The packaged runtime SHALL locate shipped assets (configuration and shaders) without relying on the current working directory.

#### Scenario: AppImage provides a stable resource root
- **GIVEN** the Goggles AppImage is executed from an arbitrary working directory
- **WHEN** the viewer loads its default configuration, UI font asset, and default shader preset
- **THEN** the viewer SHALL locate shipped assets via a stable resource root (e.g. provided by the AppImage wrapper)
- **AND** it SHALL NOT require `./shaders` to exist in the working directory

### Requirement: Optional Shader Pack Install Location

The packaging SHALL provide a way to install/update the full RetroArch shader pack (slang-shaders) into a stable user location without requiring Pixi.

#### Scenario: Shader pack is fetched into XDG data
- **WHEN** the user invokes the AppImage shader fetch/update flow
- **THEN** the shader pack SHALL be installed under `${XDG_DATA_HOME:-$HOME/.local/share}/goggles/shaders/retroarch/`

