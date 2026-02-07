## ADDED Requirements

### Requirement: Post-Chain Infrastructure

The filter chain subsystem SHALL provide a generic post-chain stage that executes after RetroArch passes and before final swapchain presentation.

#### Scenario: Post-chain as vector of passes

- **GIVEN** `FilterChain` is initialized
- **THEN** it SHALL maintain `m_postchain_passes` as a vector of `Pass` pointers
- **AND** it SHALL maintain `m_postchain_framebuffers` as a vector of `Framebuffer` pointers
- **AND** the vectors SHALL have the same count (minus one for final output)

#### Scenario: Post-chain execution order

- **GIVEN** post-chain contains N passes
- **WHEN** `record_postchain()` is called
- **THEN** passes SHALL execute in vector order (0 to N-1)
- **AND** each pass output SHALL become the next pass input
- **AND** the final pass SHALL render to the swapchain

#### Scenario: OutputPass as final post-chain entry

- **GIVEN** `FilterChain` is initialized
- **THEN** `OutputPass` SHALL be added as the last entry in `m_postchain_passes`
- **AND** it SHALL always be present (minimum post-chain size is 1)
- **AND** no framebuffer SHALL be allocated for the final pass (renders to swapchain)

#### Scenario: Post-chain extensibility

- **GIVEN** a post-processing effect is needed after RetroArch passes
- **WHEN** a new pass is added to `m_postchain_passes` before OutputPass
- **THEN** it SHALL receive the RetroArch chain output as input
- **AND** its output SHALL be passed to subsequent post-chain passes

## MODIFIED Requirements

### Requirement: OutputPass Behavior

The `OutputPass` SHALL serve as the final post-chain pass, rendering to the swapchain.

#### Scenario: OutputPass in post-chain vector

- **GIVEN** `FilterChain` is initialized
- **THEN** `OutputPass` SHALL be stored in `m_postchain_passes` vector
- **AND** there SHALL NOT be a separate `m_output_pass` member pointer
- **AND** `m_postchain_passes.back()` SHALL reference the OutputPass

#### Scenario: Direct DMA-BUF to swapchain (no RetroArch passes)

- **GIVEN** no RetroArch shader passes are configured
- **WHEN** OutputPass processes a frame
- **THEN** it SHALL sample `ctx.source_texture` (the pre-chain output or DMA-BUF import)
- **AND** begin dynamic rendering with `ctx.target_image_view`
- **AND** use `ctx.frame_index` for descriptor set selection

#### Scenario: Post-RetroArch to swapchain

- **GIVEN** RetroArch shader passes are configured
- **WHEN** OutputPass (as final post-chain pass) processes a frame
- **THEN** it SHALL sample the previous post-chain pass output (or RetroArch output if first)
- **AND** render to the swapchain image view
