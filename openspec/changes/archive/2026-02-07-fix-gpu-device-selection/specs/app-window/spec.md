## ADDED Requirements

### Requirement: GPU Device Selection

The application SHALL allow users to select a specific GPU and SHALL improve automatic GPU selection for multi-GPU systems.

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
