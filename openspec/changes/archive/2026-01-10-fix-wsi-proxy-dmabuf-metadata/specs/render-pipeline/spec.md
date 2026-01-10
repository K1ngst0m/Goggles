## ADDED Requirements

### Requirement: DMA-BUF Import Uses Exported Plane Layout

The render backend SHALL import DMA-BUF textures using the plane layout metadata provided by the capture layer.

#### Scenario: Import explicit modifier + offset
- **GIVEN** the capture layer provides a DMA-BUF FD with `stride`, `offset`, and `modifier`
- **WHEN** the viewer imports the DMA-BUF via `VkImageDrmFormatModifierExplicitCreateInfoEXT`
- **THEN** the render backend SHALL set `VkSubresourceLayout.rowPitch` to the provided `stride`
- **AND** it SHALL set `VkSubresourceLayout.offset` to the provided `offset`
- **AND** it SHALL set `drmFormatModifier` to the provided `modifier`

