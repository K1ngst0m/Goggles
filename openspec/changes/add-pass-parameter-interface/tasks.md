# Tasks

## 1. Add virtual methods to Pass interface
- [ ] Add `get_shader_parameters()` virtual method with empty default
- [ ] Add `set_shader_parameter(name, value)` virtual method with empty default
- [ ] Forward-declare or include ShaderParameter type in pass.hpp

**Validation:** Build succeeds, existing passes compile without modification

## 2. Implement interface in FilterPass
- [ ] Override `get_shader_parameters()` to return `m_parameters`
- [ ] Override `set_shader_parameter()` to update `m_parameter_overrides`
- [ ] Mark UBO dirty when parameter changes (trigger `update_ubo_parameters()`)

**Validation:** Existing shader parameter controls continue to work

## 3. Add UI helper for pass parameter rendering
- [ ] Create `draw_pass_parameters(Pass*)` helper in imgui_layer.cpp
- [ ] Render sliders for each parameter (min/max/step from metadata)
- [ ] Call `set_shader_parameter()` on value change

**Validation:** Pre-chain section shows parameters when pass has them

## 4. Wire up parameter rendering in shader stage sections
- [ ] Add parameter rendering call to Pre-Chain section
- [ ] Add parameter rendering call to Post-Chain section
- [ ] Skip rendering if `get_shader_parameters()` returns empty

**Validation:** Manual test: parameters appear only for passes that have them
