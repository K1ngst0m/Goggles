#include "filter_pass.hpp"

#include <array>
#include <render/shader/shader_runtime.hpp>
#include <util/logging.hpp>

namespace goggles::render {

FilterPass::~FilterPass() {
    FilterPass::shutdown();
}

auto FilterPass::init(vk::Device device, vk::Format target_format, uint32_t num_sync_indices,
                      ShaderRuntime& /*shader_runtime*/,
                      const std::filesystem::path& /*shader_dir*/) -> Result<void> {
    // Standard init interface - not used for FilterPass
    // Use init_from_sources or init_from_preset instead
    m_device = device;
    m_target_format = target_format;
    m_num_sync_indices = num_sync_indices;
    return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                            "FilterPass::init() not supported - use init_from_sources()");
}

auto FilterPass::init_from_sources(vk::Device device, vk::Format target_format,
                                    uint32_t num_sync_indices, ShaderRuntime& shader_runtime,
                                    const std::string& vertex_source,
                                    const std::string& fragment_source,
                                    const std::string& shader_name,
                                    FilterMode filter_mode) -> Result<void> {
    if (m_initialized) {
        return {};
    }

    m_device = device;
    m_target_format = target_format;
    m_num_sync_indices = num_sync_indices;

    // Compile RetroArch shader
    auto compile_result =
        shader_runtime.compile_retroarch_shader(vertex_source, fragment_source, shader_name);
    if (!compile_result) {
        return make_error<void>(ErrorCode::SHADER_COMPILE_FAILED, compile_result.error().message);
    }

    m_vertex_reflection = std::move(compile_result->vertex_reflection);
    m_fragment_reflection = std::move(compile_result->fragment_reflection);

    // Check if shader uses push constants
    m_has_push_constants =
        m_vertex_reflection.push_constants.has_value() ||
        m_fragment_reflection.push_constants.has_value();

    auto result = create_sampler(filter_mode);
    if (!result) {
        return result;
    }

    result = create_descriptor_resources();
    if (!result) {
        return result;
    }

    result = create_pipeline_layout();
    if (!result) {
        return result;
    }

    result = create_pipeline(compile_result->vertex_spirv, compile_result->fragment_spirv);
    if (!result) {
        return result;
    }

    m_initialized = true;
    GOGGLES_LOG_DEBUG("FilterPass '{}' initialized (push_constants={})", shader_name,
                      m_has_push_constants);
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
    m_descriptor_sets.clear();
    m_target_format = vk::Format::eUndefined;
    m_device = nullptr;
    m_num_sync_indices = 0;
    m_has_push_constants = false;

    m_initialized = false;
    GOGGLES_LOG_DEBUG("FilterPass shutdown");
}

void FilterPass::update_descriptor(uint32_t frame_index, vk::ImageView source_view) {
    vk::DescriptorImageInfo image_info{};
    image_info.sampler = *m_sampler;
    image_info.imageView = source_view;
    image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::WriteDescriptorSet write{};
    write.dstSet = m_descriptor_sets[frame_index];
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo = &image_info;

    m_device.updateDescriptorSets(write, {});
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

    // Push semantic values if shader uses push constants
    if (m_has_push_constants) {
        auto push_data = m_binder.get_push_constants();
        cmd.pushConstants(*m_pipeline_layout,
                          vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
                          sizeof(RetroArchPushConstants), &push_data);
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

    cmd.draw(3, 1, 0, 0);
    cmd.endRendering();
}

auto FilterPass::create_sampler(FilterMode filter_mode) -> Result<void> {
    vk::Filter filter =
        (filter_mode == FilterMode::LINEAR) ? vk::Filter::eLinear : vk::Filter::eNearest;

    vk::SamplerCreateInfo create_info{};
    create_info.magFilter = filter;
    create_info.minFilter = filter;
    create_info.mipmapMode = vk::SamplerMipmapMode::eNearest;
    create_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    create_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    create_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    create_info.mipLodBias = 0.0F;
    create_info.anisotropyEnable = VK_FALSE;
    create_info.compareEnable = VK_FALSE;
    create_info.minLod = 0.0F;
    create_info.maxLod = 0.0F;
    create_info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
    create_info.unnormalizedCoordinates = VK_FALSE;

    auto [result, sampler] = m_device.createSamplerUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create sampler: " + vk::to_string(result));
    }

    m_sampler = std::move(sampler);
    return {};
}

auto FilterPass::create_descriptor_resources() -> Result<void> {
    // Combined image sampler for Source texture (binding 0)
    vk::DescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    binding.descriptorCount = 1;
    binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo layout_info{};
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;

    auto [layout_result, layout] = m_device.createDescriptorSetLayoutUnique(layout_info);
    if (layout_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create descriptor set layout: " +
                                    vk::to_string(layout_result));
    }
    m_descriptor_layout = std::move(layout);

    vk::DescriptorPoolSize pool_size{};
    pool_size.type = vk::DescriptorType::eCombinedImageSampler;
    pool_size.descriptorCount = m_num_sync_indices;

    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.maxSets = m_num_sync_indices;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;

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

    // Add push constant range if needed
    vk::PushConstantRange push_range{};
    if (m_has_push_constants) {
        push_range.stageFlags =
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        push_range.offset = 0;
        push_range.size = sizeof(RetroArchPushConstants);
        create_info.pushConstantRangeCount = 1;
        create_info.pPushConstantRanges = &push_range;
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

} // namespace goggles::render
