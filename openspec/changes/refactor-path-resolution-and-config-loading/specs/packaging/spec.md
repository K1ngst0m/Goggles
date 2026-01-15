# packaging Specification (Delta)

## MODIFIED Requirements

### Requirement: Packaged Assets Are Not CWD-Dependent
The packaged runtime SHALL locate shipped assets (configuration templates and shader assets) without
relying on the current working directory.

#### Scenario: AppImage provides a stable resource root
- **GIVEN** the Goggles AppImage is executed from an arbitrary working directory
- **WHEN** the viewer loads its default configuration template and shader assets
- **THEN** the viewer SHALL locate shipped assets via a stable `resource_dir` resolution rule
- **AND** it SHALL NOT require `./config` or `./shaders` to exist in the working directory

