## ADDED Requirements

### Requirement: Toolchain Version Pinning

The build system SHALL pin all development tool versions in pixi.toml to prevent system tool leakage and ensure reproducible builds.

#### Scenario: Clang toolchain version consistency
- **WHEN** building with pixi
- **THEN** clang, clang++, lld, and clang-tools SHALL use the same major version (21.x)

#### Scenario: Build tool version pinning
- **WHEN** pixi.toml specifies cmake, ninja, ccache
- **THEN** each tool SHALL have a pinned version constraint (not `*`)

#### Scenario: Format tool version consistency
- **WHEN** running format tasks in default or lint environment
- **THEN** taplo version SHALL be identical across environments
