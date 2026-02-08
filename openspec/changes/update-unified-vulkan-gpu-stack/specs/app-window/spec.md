## MODIFIED Requirements

### Requirement: GPU Device Selection
The application SHALL allow users to select a specific GPU and SHALL enforce a unified GPU policy across viewer, compositor, and spawned target app processes.

#### Scenario: Explicit GPU selection by index
- **GIVEN** multiple GPUs are available
- **WHEN** the user runs with `--gpu 0`
- **THEN** the application SHALL use the GPU at index 0

#### Scenario: Explicit GPU selection by name
- **GIVEN** multiple GPUs are available including one with "AMD" in its name
- **WHEN** the user runs with `--gpu AMD`
- **THEN** the application SHALL use the GPU whose name contains "AMD"

#### Scenario: Invalid GPU selector
- **GIVEN** no GPU matches the selector
- **WHEN** the user runs with `--gpu nonexistent`
- **THEN** the application SHALL exit with an error message listing available GPUs

#### Scenario: Ambiguous GPU selector
- **GIVEN** multiple suitable GPUs match the name selector
- **WHEN** the user runs with a non-unique selector like `--gpu AMD`
- **THEN** the application SHALL exit with an error listing matching GPUs
- **AND** it SHALL instruct the user to choose a numeric index

#### Scenario: Default GPU selection
- **GIVEN** multiple GPUs are available and no `--gpu` option is specified
- **WHEN** the application selects a GPU
- **THEN** it SHALL prefer a GPU that supports presenting to the current surface
- **AND** it SHALL log all available GPUs with their indices

#### Scenario: Canonical UUID to DRM node resolution
- **GIVEN** a GPU is selected for the viewer Vulkan backend
- **WHEN** runtime startup policy is finalized
- **THEN** the application SHALL resolve that GPU UUID to an accessible DRM render node path
- **AND** it SHALL use that mapping to initialize compositor renderer selection

#### Scenario: Unified policy hard-fail
- **GIVEN** one or more subsystems cannot honor the selected GPU UUID
- **WHEN** startup validation runs
- **THEN** the application SHALL fail startup
- **AND** it SHALL report which subsystem violated the unified policy

#### Scenario: ICD identity propagation
- **GIVEN** the runtime GPU policy is active
- **WHEN** the target app process is spawned
- **THEN** the same ICD environment identity used by the viewer/compositor SHALL be propagated
- **AND** the layer process context SHALL observe the same ICD identity
