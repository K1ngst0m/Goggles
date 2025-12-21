#include "filter_pass.hpp"

#include <array>
#include <cstring>
#include <render/shader/shader_runtime.hpp>
#include <util/logging.hpp>

namespace goggles::render {

namespace {

auto convert_wrap_mode(WrapMode mode) -> vk::SamplerAddressMode {
    switch (mode) {
    case WrapMode::CLAMP_TO_EDGE:
        return vk::SamplerAddressMode::eClampToEdge;
    case WrapMode::REPEAT:
        return vk::SamplerAddressMode::eRepeat;
    case WrapMode::MIRRORED_REPEAT:
        return vk::SamplerAddressMode::eMirroredRepeat;
    case WrapMode::CLAMP_TO_BORDER:
    default:
        return vk::SamplerAddressMode::eClampToBorder;
    }
}

constexpr std::array<Vertex, 6> FULLSCREEN_QUAD_VERTICES = {{
    {.position = {-1.0F, -1.0F, 0.0F, 1.0F}, .texcoord = {0.0F, 0.0F}},
    {.position = {1.0F, -1.0F, 0.0F, 1.0F}, .texcoord = {1.0F, 0.0F}},
    {.position = {1.0F, 1.0F, 0.0F, 1.0F}, .texcoord = {1.0F, 1.0F}},
    {.position = {-1.0F, -1.0F, 0.0F, 1.0F}, .texcoord = {0.0F, 0.0F}},
    {.position = {1.0F, 1.0F, 0.0F, 1.0F}, .texcoord = {1.0F, 1.0F}},
    {.position = {-1.0F, 1.0F, 0.0F, 1.0F}, .texcoord = {0.0F, 1.0F}},
}};

} // namespace

FilterPass::~FilterPass() {
    FilterPass::shutdown();
}

auto FilterPass::init(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                      const FilterPassConfig& config) -> Result<void> {
    if (m_initialized) {
        return {};
    }

    m_device = vk_ctx.device;
    m_physical_device = vk_ctx.physical_device;
    m_target_format = config.target_format;
    m_num_sync_indices = config.num_sync_indices;
    m_parameters = config.parameters;

    auto compile_result = shader_runtime.compile_retroarch_shader(
        config.vertex_source, config.fragment_source, config.shader_name);
    if (!compile_result) {
        return make_error<void>(ErrorCode::SHADER_COMPILE_FAILED, compile_result.error().message);
    }

    m_vertex_reflection = std::move(compile_result->vertex_reflection);
    m_fragment_reflection = std::move(compile_result->fragment_reflection);
    m_merged_reflection = merge_reflection(m_vertex_reflection, m_fragment_reflection);

    m_has_push_constants = m_merged_reflection.push_constants.has_value();
    m_has_vertex_inputs = !m_merged_reflection.vertex_inputs.empty();

    if (m_has_push_constants) {
        m_push_constant_size =
            static_cast<uint32_t>(m_merged_reflection.push_constants->total_size);
        m_push_data.resize(m_push_constant_size, 0);
        GOGGLES_LOG_DEBUG("Push constant size from reflection: {} bytes", m_push_constant_size);

        for (const auto& member : m_merged_reflection.push_constants->members) {
            GOGGLES_LOG_DEBUG("  Push constant member: '{}' offset={} size={}", member.name,
                              member.offset, member.size);
        }
    }

    GOGGLES_LOG_DEBUG("FilterPass parameters count: {}", m_parameters.size());
    for (const auto& param : m_parameters) {
        GOGGLES_LOG_DEBUG("  Param: '{}' default={}", param.name, param.default_value);
    }

    GOGGLES_TRY(create_sampler(config.filter_mode, config.mipmap, config.wrap_mode));

    if (m_has_vertex_inputs) {
        GOGGLES_TRY(create_vertex_buffer());
    }

    GOGGLES_TRY(create_ubo_buffer());
    GOGGLES_TRY(create_descriptor_resources());
    GOGGLES_TRY(create_pipeline_layout());
    GOGGLES_TRY(create_pipeline(compile_result->vertex_spirv, compile_result->fragment_spirv));

    m_initialized = true;
    GOGGLES_LOG_DEBUG("FilterPass '{}' initialized (push_constants={}, size={}, vertex_inputs={})",
                      config.shader_name, m_has_push_constants, m_push_constant_size,
                      m_has_vertex_inputs);
    return {};
}

void FilterPass::shutdown() {
    if (!m_initialized) {
        return;
    }

    m_pipeline.reset();
    m_pipeline_layout.reset();
    m_descriptor_pool.reset();
    m_descriptor_layout.reset();
    m_sampler.reset();
    m_vertex_buffer.reset();
    m_vertex_buffer_memory.reset();
    m_ubo_buffer.reset();
    m_ubo_memory.reset();
    m_descriptor_sets.clear();
    m_push_data.clear();
    m_parameters.clear();
    m_target_format = vk::Format::eUndefined;
    m_device = nullptr;
    m_physical_device = nullptr;
    m_num_sync_indices = 0;
    m_has_push_constants = false;
    m_has_vertex_inputs = false;
    m_has_ubo = false;
    m_push_constant_size = 0;

    m_initialized = false;
    GOGGLES_LOG_DEBUG("FilterPass shutdown");
}

void FilterPass::update_descriptor(uint32_t frame_index, vk::ImageView source_view) {
    std::vector<vk::WriteDescriptorSet> writes;
    std::vector<vk::DescriptorImageInfo> image_infos;
    vk::DescriptorBufferInfo ubo_info{};

    if (m_has_ubo && m_ubo_buffer && m_merged_reflection.ubo.has_value()) {
        ubo_info.buffer = *m_ubo_buffer;
        ubo_info.offset = 0;
        ubo_info.range = m_merged_reflection.ubo->total_size;

        vk::WriteDescriptorSet write{};
        write.dstSet = m_descriptor_sets[frame_index];
        write.dstBinding = m_merged_reflection.ubo->binding;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eUniformBuffer;
        write.pBufferInfo = &ubo_info;
        writes.push_back(write);
    }

    image_infos.reserve(m_merged_reflection.textures.size());

    for (const auto& tex : m_merged_reflection.textures) {
        vk::ImageView view = source_view;
        vk::Sampler sampler = *m_sampler;

        auto it = m_texture_bindings.find(tex.name);
        if (it != m_texture_bindings.end()) {
            view = it->second.view;
            if (it->second.sampler) {
                sampler = it->second.sampler;
            }
        }

        vk::DescriptorImageInfo image_info{};
        image_info.sampler = sampler;
        image_info.imageView = view;
        image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        image_infos.push_back(image_info);

        vk::WriteDescriptorSet write{};
        write.dstSet = m_descriptor_sets[frame_index];
        write.dstBinding = tex.binding;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write.pImageInfo = &image_infos.back();
        writes.push_back(write);
    }

    if (!writes.empty()) {
        m_device.updateDescriptorSets(writes, {});
    }
}

void FilterPass::build_push_constants() {
    if (!m_has_push_constants || m_push_data.empty() ||
        !m_merged_reflection.push_constants.has_value()) {
        return;
    }

    std::memset(m_push_data.data(), 0, m_push_data.size());

    for (const auto& member : m_merged_reflection.push_constants->members) {
        if (member.offset + member.size > m_push_data.size()) {
            continue;
        }

        auto* dest = m_push_data.data() + member.offset;

        if (member.name == "SourceSize") {
            std::memcpy(dest, m_binder.source_size().data(), sizeof(SizeVec4));
        } else if (member.name == "OriginalSize") {
            std::memcpy(dest, m_binder.original_size().data(), sizeof(SizeVec4));
        } else if (member.name == "OutputSize") {
            std::memcpy(dest, m_binder.output_size().data(), sizeof(SizeVec4));
        } else if (member.name == "FrameCount") {
            auto frame_count = m_binder.frame_count();
            std::memcpy(dest, &frame_count, sizeof(uint32_t));
        } else if (member.name == "FinalViewportSize") {
            std::memcpy(dest, m_binder.final_viewport_size().data(), sizeof(SizeVec4));
        } else if (member.name.size() > 4 && member.name.ends_with("Size")) {
            auto alias_name = member.name.substr(0, member.name.size() - 4);
            if (auto alias_size = m_binder.get_alias_size(alias_name)) {
                std::memcpy(dest, alias_size->data(), sizeof(SizeVec4));
            }
        } else {
            for (const auto& param : m_parameters) {
                if (param.name == member.name) {
                    float value = param.default_value;
                    auto override_it = m_parameter_overrides.find(param.name);
                    if (override_it != m_parameter_overrides.end()) {
                        value = override_it->second;
                    }
                    std::memcpy(dest, &value, sizeof(float));
                    break;
                }
            }
        }
    }
}

void FilterPass::record(vk::CommandBuffer cmd, const PassContext& ctx) {
    update_descriptor(ctx.frame_index, ctx.source_texture);

    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.imageView = ctx.target_image_view;
    color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.clearValue.color = vk::ClearColorValue{std::array{0.0F, 0.0F, 0.0F, 1.0F}};

    vk::RenderingInfo rendering_info{};
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    rendering_info.renderArea.extent = ctx.output_extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    cmd.beginRendering(rendering_info);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipeline_layout, 0,
                           m_descriptor_sets[ctx.frame_index], {});

    if (m_has_push_constants && m_push_constant_size > 0) {
        build_push_constants();
        cmd.pushConstants(*m_pipeline_layout,
                          vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
                          m_push_constant_size, m_push_data.data());
    }

    vk::Viewport viewport{};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(ctx.output_extent.width);
    viewport.height = static_cast<float>(ctx.output_extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    cmd.setViewport(0, viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = ctx.output_extent;
    cmd.setScissor(0, scissor);

    if (m_has_vertex_inputs && m_vertex_buffer) {
        vk::DeviceSize offset = 0;
        cmd.bindVertexBuffers(0, *m_vertex_buffer, offset);
        cmd.draw(6, 1, 0, 0);
    } else {
        cmd.draw(3, 1, 0, 0);
    }
    cmd.endRendering();
}

auto FilterPass::create_sampler(FilterMode filter_mode, bool mipmap, WrapMode wrap_mode)
    -> Result<void> {
    vk::Filter filter =
        (filter_mode == FilterMode::LINEAR) ? vk::Filter::eLinear : vk::Filter::eNearest;

    vk::SamplerMipmapMode mipmap_mode = (filter_mode == FilterMode::LINEAR)
                                            ? vk::SamplerMipmapMode::eLinear
                                            : vk::SamplerMipmapMode::eNearest;

    vk::SamplerAddressMode address_mode = convert_wrap_mode(wrap_mode);

    vk::SamplerCreateInfo create_info{};
    create_info.magFilter = filter;
    create_info.minFilter = filter;
    create_info.mipmapMode = mipmap_mode;
    create_info.addressModeU = address_mode;
    create_info.addressModeV = address_mode;
    create_info.addressModeW = address_mode;
    create_info.mipLodBias = 0.0F;
    create_info.anisotropyEnable = VK_FALSE;
    create_info.compareEnable = VK_FALSE;
    create_info.minLod = 0.0F;
    create_info.maxLod = mipmap ? VK_LOD_CLAMP_NONE : 0.0F;
    create_info.borderColor = vk::BorderColor::eFloatTransparentBlack;
    create_info.unnormalizedCoordinates = VK_FALSE;

    auto [result, sampler] = m_device.createSamplerUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create sampler: " + vk::to_string(result));
    }

    m_sampler = std::move(sampler);
    return {};
}

auto FilterPass::create_vertex_buffer() -> Result<void> {
    vk::DeviceSize buffer_size = sizeof(FULLSCREEN_QUAD_VERTICES);

    vk::BufferCreateInfo buffer_info{};
    buffer_info.size = buffer_size;
    buffer_info.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;

    auto [buf_result, buffer] = m_device.createBufferUnique(buffer_info);
    if (buf_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create vertex buffer: " + vk::to_string(buf_result));
    }

    auto mem_reqs = m_device.getBufferMemoryRequirements(*buffer);

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex =
        find_memory_type(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible |
                                                      vk::MemoryPropertyFlagBits::eHostCoherent);

    auto [mem_result, memory] = m_device.allocateMemoryUnique(alloc_info);
    if (mem_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to allocate vertex buffer memory: " +
                                    vk::to_string(mem_result));
    }

    auto bind_result = m_device.bindBufferMemory(*buffer, *memory, 0);
    if (bind_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to bind vertex buffer memory: " +
                                    vk::to_string(bind_result));
    }

    auto [map_result, data] = m_device.mapMemory(*memory, 0, buffer_size);
    if (map_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to map vertex buffer memory: " + vk::to_string(map_result));
    }
    std::memcpy(data, FULLSCREEN_QUAD_VERTICES.data(), buffer_size);
    m_device.unmapMemory(*memory);

    m_vertex_buffer = std::move(buffer);
    m_vertex_buffer_memory = std::move(memory);
    return {};
}

auto FilterPass::create_ubo_buffer() -> Result<void> {
    if (!m_merged_reflection.ubo.has_value()) {
        return {};
    }

    m_has_ubo = true;
    m_ubo_size = m_merged_reflection.ubo->total_size;

    for (const auto& member : m_merged_reflection.ubo->members) {
        m_ubo_member_offsets[member.name] = member.offset;
    }

    vk::BufferCreateInfo buffer_info{};
    buffer_info.size = m_ubo_size;
    buffer_info.usage = vk::BufferUsageFlagBits::eUniformBuffer;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;

    auto [buf_result, buffer] = m_device.createBufferUnique(buffer_info);
    if (buf_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create UBO buffer: " + vk::to_string(buf_result));
    }

    auto mem_reqs = m_device.getBufferMemoryRequirements(*buffer);

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex =
        find_memory_type(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible |
                                                      vk::MemoryPropertyFlagBits::eHostCoherent);

    auto [mem_result, memory] = m_device.allocateMemoryUnique(alloc_info);
    if (mem_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to allocate UBO memory: " + vk::to_string(mem_result));
    }

    auto bind_result = m_device.bindBufferMemory(*buffer, *memory, 0);
    if (bind_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to bind UBO memory: " + vk::to_string(bind_result));
    }

    auto [map_result, data] = m_device.mapMemory(*memory, 0, m_ubo_size);
    if (map_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to map UBO memory: " + vk::to_string(map_result));
    }

    auto ubo_data = m_binder.get_ubo();
    std::memcpy(data, &ubo_data, std::min(m_ubo_size, sizeof(ubo_data)));
    m_device.unmapMemory(*memory);

    m_ubo_buffer = std::move(buffer);
    m_ubo_memory = std::move(memory);

    GOGGLES_LOG_DEBUG("UBO buffer created, size={}, members={}", m_ubo_size,
                      m_ubo_member_offsets.size());
    return {};
}

auto FilterPass::find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties)
    -> uint32_t {
    auto mem_props = m_physical_device.getMemoryProperties();
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1U << i)) != 0U &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

auto FilterPass::create_descriptor_resources() -> Result<void> {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;

    if (m_merged_reflection.ubo.has_value()) {
        vk::DescriptorSetLayoutBinding ubo_binding{};
        ubo_binding.binding = m_merged_reflection.ubo->binding;
        ubo_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        ubo_binding.descriptorCount = 1;
        ubo_binding.stageFlags = m_merged_reflection.ubo->stage_flags;
        bindings.push_back(ubo_binding);
        GOGGLES_LOG_DEBUG("Descriptor binding {}: UBO, stages={}", ubo_binding.binding,
                          vk::to_string(ubo_binding.stageFlags));
    }

    for (const auto& tex : m_merged_reflection.textures) {
        vk::DescriptorSetLayoutBinding tex_binding{};
        tex_binding.binding = tex.binding;
        tex_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        tex_binding.descriptorCount = 1;
        tex_binding.stageFlags = tex.stage_flags;
        bindings.push_back(tex_binding);
        GOGGLES_LOG_DEBUG("Descriptor binding {}: texture '{}', stages={}", tex_binding.binding,
                          tex.name, vk::to_string(tex_binding.stageFlags));
    }

    if (bindings.empty()) {
        vk::DescriptorSetLayoutBinding fallback{};
        fallback.binding = 0;
        fallback.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        fallback.descriptorCount = 1;
        fallback.stageFlags = vk::ShaderStageFlagBits::eFragment;
        bindings.push_back(fallback);
    }

    vk::DescriptorSetLayoutCreateInfo layout_info{};
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    auto [layout_result, layout] = m_device.createDescriptorSetLayoutUnique(layout_info);
    if (layout_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create descriptor set layout: " +
                                    vk::to_string(layout_result));
    }
    m_descriptor_layout = std::move(layout);

    std::vector<vk::DescriptorPoolSize> pool_sizes;

    uint32_t ubo_count = m_merged_reflection.ubo.has_value() ? m_num_sync_indices : 0;
    uint32_t sampler_count =
        static_cast<uint32_t>(m_merged_reflection.textures.size()) * m_num_sync_indices;

    if (sampler_count == 0) {
        sampler_count = m_num_sync_indices;
    }

    if (ubo_count > 0) {
        pool_sizes.emplace_back(vk::DescriptorType::eUniformBuffer, ubo_count);
    }
    pool_sizes.emplace_back(vk::DescriptorType::eCombinedImageSampler, sampler_count);

    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.maxSets = m_num_sync_indices;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    auto [pool_result, pool] = m_device.createDescriptorPoolUnique(pool_info);
    if (pool_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create descriptor pool: " + vk::to_string(pool_result));
    }
    m_descriptor_pool = std::move(pool);

    std::vector<vk::DescriptorSetLayout> layouts(m_num_sync_indices, *m_descriptor_layout);

    vk::DescriptorSetAllocateInfo alloc_info{};
    alloc_info.descriptorPool = *m_descriptor_pool;
    alloc_info.descriptorSetCount = m_num_sync_indices;
    alloc_info.pSetLayouts = layouts.data();

    auto [alloc_result, sets] = m_device.allocateDescriptorSets(alloc_info);
    if (alloc_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to allocate descriptor sets: " +
                                    vk::to_string(alloc_result));
    }
    m_descriptor_sets = std::move(sets);

    return {};
}

auto FilterPass::create_pipeline_layout() -> Result<void> {
    vk::PipelineLayoutCreateInfo create_info{};
    create_info.setLayoutCount = 1;
    create_info.pSetLayouts = &*m_descriptor_layout;

    vk::PushConstantRange push_range{};
    if (m_has_push_constants && m_push_constant_size > 0) {
        push_range.stageFlags =
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        push_range.offset = 0;
        push_range.size = m_push_constant_size;
        create_info.pushConstantRangeCount = 1;
        create_info.pPushConstantRanges = &push_range;
        GOGGLES_LOG_DEBUG("Pipeline layout push constant range: {} bytes", m_push_constant_size);
    }

    auto [result, layout] = m_device.createPipelineLayoutUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create pipeline layout: " + vk::to_string(result));
    }

    m_pipeline_layout = std::move(layout);
    return {};
}

auto FilterPass::create_pipeline(const std::vector<uint32_t>& vertex_spirv,
                                 const std::vector<uint32_t>& fragment_spirv) -> Result<void> {
    vk::ShaderModuleCreateInfo vert_module_info{};
    vert_module_info.codeSize = vertex_spirv.size() * sizeof(uint32_t);
    vert_module_info.pCode = vertex_spirv.data();

    auto [vert_mod_result, vert_module] = m_device.createShaderModuleUnique(vert_module_info);
    if (vert_mod_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create vertex shader module: " +
                                    vk::to_string(vert_mod_result));
    }

    vk::ShaderModuleCreateInfo frag_module_info{};
    frag_module_info.codeSize = fragment_spirv.size() * sizeof(uint32_t);
    frag_module_info.pCode = fragment_spirv.data();

    auto [frag_mod_result, frag_module] = m_device.createShaderModuleUnique(frag_module_info);
    if (frag_mod_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create fragment shader module: " +
                                    vk::to_string(frag_mod_result));
    }

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *vert_module;
    stages[0].pName = "main";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *frag_module;
    stages[1].pName = "main";

    vk::PipelineVertexInputStateCreateInfo vertex_input{};
    vk::VertexInputBindingDescription binding_desc{};
    std::vector<vk::VertexInputAttributeDescription> attrib_descs;

    if (m_has_vertex_inputs) {
        binding_desc.binding = 0;
        binding_desc.stride = sizeof(Vertex);
        binding_desc.inputRate = vk::VertexInputRate::eVertex;

        for (const auto& input : m_merged_reflection.vertex_inputs) {
            vk::VertexInputAttributeDescription attrib{};
            attrib.location = input.location;
            attrib.binding = 0;
            attrib.format = input.format;
            attrib.offset = input.offset;
            attrib_descs.push_back(attrib);
            GOGGLES_LOG_DEBUG("Vertex input location {}: format={}, offset={}", input.location,
                              vk::to_string(input.format), input.offset);
        }

        vertex_input.vertexBindingDescriptionCount = 1;
        vertex_input.pVertexBindingDescriptions = &binding_desc;
        vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrib_descs.size());
        vertex_input.pVertexAttributeDescriptions = attrib_descs.data();
    }

    vk::PipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    vk::PipelineViewportStateCreateInfo viewport_state{};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterization{};
    rasterization.depthClampEnable = VK_FALSE;
    rasterization.rasterizerDiscardEnable = VK_FALSE;
    rasterization.polygonMode = vk::PolygonMode::eFill;
    rasterization.cullMode = vk::CullModeFlagBits::eNone;
    rasterization.frontFace = vk::FrontFace::eCounterClockwise;
    rasterization.depthBiasEnable = VK_FALSE;
    rasterization.lineWidth = 1.0F;

    vk::PipelineMultisampleStateCreateInfo multisample{};
    multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisample.sampleShadingEnable = VK_FALSE;

    vk::PipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.blendEnable = VK_FALSE;
    blend_attachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo color_blend{};
    color_blend.logicOpEnable = VK_FALSE;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &blend_attachment;

    std::array dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    vk::PipelineRenderingCreateInfo rendering_info{};
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachmentFormats = &m_target_format;
    rendering_info.depthAttachmentFormat = vk::Format::eUndefined;
    rendering_info.stencilAttachmentFormat = vk::Format::eUndefined;

    vk::GraphicsPipelineCreateInfo create_info{};
    create_info.pNext = &rendering_info;
    create_info.stageCount = static_cast<uint32_t>(stages.size());
    create_info.pStages = stages.data();
    create_info.pVertexInputState = &vertex_input;
    create_info.pInputAssemblyState = &input_assembly;
    create_info.pViewportState = &viewport_state;
    create_info.pRasterizationState = &rasterization;
    create_info.pMultisampleState = &multisample;
    create_info.pColorBlendState = &color_blend;
    create_info.pDynamicState = &dynamic_state;
    create_info.layout = *m_pipeline_layout;

    auto [result, pipelines] = m_device.createGraphicsPipelinesUnique(nullptr, create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create graphics pipeline: " + vk::to_string(result));
    }

    m_pipeline = std::move(pipelines[0]);
    return {};
}

auto FilterPass::update_ubo_parameters() -> Result<void> {
    if (!m_has_ubo || !m_ubo_memory || m_ubo_size == 0) {
        return {};
    }

    auto [map_result, data] = m_device.mapMemory(*m_ubo_memory, 0, m_ubo_size);
    if (map_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to map UBO memory: " + vk::to_string(map_result));
    }

    auto* ubo_data = static_cast<char*>(data);

    for (const auto& param : m_parameters) {
        auto it = m_ubo_member_offsets.find(param.name);
        if (it != m_ubo_member_offsets.end()) {
            float value = param.default_value;
            auto override_it = m_parameter_overrides.find(param.name);
            if (override_it != m_parameter_overrides.end()) {
                value = override_it->second;
            }
            std::memcpy(ubo_data + it->second, &value, sizeof(float));
        }
    }

    m_device.unmapMemory(*m_ubo_memory);
    return {};
}

} // namespace goggles::render
