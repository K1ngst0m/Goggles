## ADDED Requirements

### Requirement: Layer Honors Unified GPU Policy
When capture mode is enabled, the layer path SHALL honor the same GPU UUID and ICD identity policy selected by the Goggles runtime.

#### Scenario: UUID policy inherited from launcher
- **GIVEN** Goggles launches a target app with `GOGGLES_GPU_UUID` set
- **WHEN** layer initialization enumerates physical devices
- **THEN** the layer SHALL restrict capture device selection to that UUID

#### Scenario: ICD policy inherited from launcher
- **GIVEN** Goggles launches a target app with explicit ICD environment settings
- **WHEN** Vulkan loader resolves ICDs in the target process
- **THEN** layer-managed capture SHALL run under the same ICD identity as Goggles runtime policy

#### Scenario: Policy mismatch handling
- **GIVEN** layer-side device selection cannot satisfy UUID or ICD policy constraints
- **WHEN** capture initialization runs
- **THEN** capture setup SHALL fail safely
- **AND** it SHALL not silently switch to a different GPU
