# render-pipeline Spec Delta

## ADDED Requirements

### Requirement: Pass Initialization Interface

All render pass classes SHALL use a consistent initialization pattern with typed config structs and shared Vulkan context.

#### Scenario: VulkanContext sharing

- **GIVEN** `VulkanBackend` has initialized device and physical device
- **WHEN** any pass is initialized
- **THEN** the pass SHALL receive a `VulkanContext` reference containing both handles
- **AND** the pass SHALL NOT store redundant copies of device handles

#### Scenario: OutputPass initialization with config

- **GIVEN** an `OutputPassConfig` with target format, sync indices, and shader directory
- **WHEN** `OutputPass::init()` is called with `VulkanContext`, `ShaderRuntime`, and config
- **THEN** the pass SHALL initialize using the provided configuration
- **AND** the signature SHALL be `init(const VulkanContext&, ShaderRuntime&, const OutputPassConfig&)`

#### Scenario: FilterPass initialization with config

- **GIVEN** a `FilterPassConfig` with target format, sync indices, shader sources, and filter mode
- **WHEN** `FilterPass::init()` is called with `VulkanContext`, `ShaderRuntime`, and config
- **THEN** the pass SHALL compile shaders and create pipeline from the config
- **AND** the signature SHALL be `init(const VulkanContext&, ShaderRuntime&, const FilterPassConfig&)`

## MODIFIED Requirements

### Requirement: Fullscreen Blit Pipeline

The render backend SHALL provide a graphics pipeline for blitting imported textures to the swapchain, initialized via typed config structs.

#### Scenario: Pipeline initialization

- **GIVEN** valid SPIR-V bytecode from `ShaderRuntime`
- **WHEN** `OutputPass` is initialized via `init(const VulkanContext&, ShaderRuntime&, const OutputPassConfig&)`
- **THEN** pipeline and descriptor layout SHALL be created
- **AND** pipeline SHALL be created with `VkPipelineRenderingCreateInfo` specifying target format from config
- **AND** all Vulkan resources SHALL use RAII (`vk::Unique*`)
