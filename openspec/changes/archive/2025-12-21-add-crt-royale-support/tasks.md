## 1. Implementation
- [x] 1.1 Add PNG texture loader + GPU upload utility (staging + optional mipmap generation)
- [x] 1.2 Extend `PresetParser` to parse `*_wrap_mode` and store in `TextureConfig`
- [x] 1.3 Add texture registry to `FilterChain` and load preset textures at `load_preset()`
- [x] 1.4 Add alias routing and size map for `ALIASSize` push constants
- [x] 1.5 Update `FilterPass` to bind textures by name (Source/Original/aliases/LUTs)
- [x] 1.6 Populate UBO parameter values by reflection name, applying preset overrides
- [x] 1.7 Add/extend tests for texture parsing, alias size mapping, and UBO parameter binding
