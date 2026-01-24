# Tasks

## 1. Add virtual methods to Pass interface
- [x] Add `get_shader_parameters()` virtual method with empty default
- [x] Add `set_shader_parameter(name, value)` virtual method with empty default
- [x] Forward-declare or include ShaderParameter type in pass.hpp

**Validation:** Build succeeds, existing passes compile without modification

## 2. Implement interface in FilterPass
- [x] Override `get_shader_parameters()` to return `m_parameters`
- [x] Override `set_shader_parameter()` to update `m_parameter_overrides`
- [x] Mark UBO dirty when parameter changes (trigger `update_ubo_parameters()`)

**Validation:** Existing shader parameter controls continue to work

## 3. Add Gaussian filter to downsample shader
- [x] Add `filter_type` push constant to downsample.frag.slang
- [x] Implement Gaussian bilinear sampling path (4 taps, 16 texels effective)
- [x] Branch on filter_type: 0=area (current), 1=gaussian

**Validation:** Shader compiles, visual comparison shows smoother output for gaussian

## 4. Implement parameter interface in DownsamplePass
- [x] Add `m_filter_type` member variable (default 0.0)
- [x] Override `get_shader_parameters()` to return filter_type metadata
- [x] Override `set_shader_parameter()` to update m_filter_type
- [x] Pass filter_type to shader via push constants

**Validation:** Filter type changes at runtime without rebuild

## 5. Add UI helper for pass parameter rendering
- [x] Create `draw_pass_parameters(Pass*)` helper in imgui_layer.cpp
- [x] Render combo box for enum-style parameters (filter_type)
- [x] Render sliders for continuous parameters
- [x] Call `set_shader_parameter()` on value change

**Validation:** Pre-chain section shows filter type dropdown

## 6. Wire up parameter rendering in shader stage sections
- [x] Add parameter rendering call to Pre-Chain section
- [x] Add parameter rendering call to Post-Chain section
- [x] Skip rendering if `get_shader_parameters()` returns empty

**Validation:** Manual test: filter type dropdown appears, switching works
