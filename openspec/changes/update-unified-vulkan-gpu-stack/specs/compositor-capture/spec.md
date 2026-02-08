## ADDED Requirements

### Requirement: Compositor Uses Explicit Vulkan Renderer
The compositor capture path SHALL create a wlroots Vulkan renderer explicitly using a resolved DRM render node FD.

#### Scenario: Vulkan renderer creation succeeds
- **GIVEN** a valid selected GPU policy with resolved DRM render node
- **WHEN** compositor initialization runs
- **THEN** it SHALL call wlroots Vulkan renderer creation with that DRM FD
- **AND** renderer initialization SHALL succeed only if the renderer backend is Vulkan

#### Scenario: Vulkan renderer creation fails
- **GIVEN** compositor mode is enabled
- **WHEN** Vulkan renderer creation fails
- **THEN** compositor initialization SHALL fail
- **AND** it SHALL NOT fall back to GLES renderer

### Requirement: Compositor GPU Identity Matches Viewer Policy
The compositor capture path SHALL use the same selected GPU identity as the viewer Vulkan backend.

#### Scenario: Matching UUID and render node
- **GIVEN** the viewer selects a GPU UUID
- **WHEN** the compositor initializes
- **THEN** the compositor render node mapping SHALL resolve to the same physical GPU identity
- **AND** startup logs SHALL include the selected UUID and render node path

#### Scenario: UUID to render node mapping failure
- **GIVEN** the selected UUID cannot be resolved to an accessible DRM render node
- **WHEN** compositor initialization runs
- **THEN** startup SHALL fail with a mapping error

### Requirement: Compositor Present Modifier Compatibility Fallback
The compositor present path SHALL preserve LINEAR modifier fallback for DMA-BUF buffers when common non-linear modifiers are not safely available.

#### Scenario: Preferred modifier unsupported
- **GIVEN** no mutually compatible non-linear modifier is available for compositor presentation buffers
- **WHEN** the present swapchain is created
- **THEN** the compositor SHALL allocate buffers with LINEAR modifier
- **AND** presentation SHALL continue without GPU mismatch fallback

#### Scenario: Preferred modifier available
- **GIVEN** a mutually compatible non-linear modifier is available
- **WHEN** the present swapchain is created
- **THEN** the compositor MAY select that modifier
- **AND** it SHALL still support fallback to LINEAR on later import/negotiation failure
