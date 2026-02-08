#include "output_pass.hpp"

#include <array>
#include <render/shader/shader_runtime.hpp>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render {

OutputPass::~OutputPass() {
    OutputPass::shutdown();
}

auto OutputPass::create(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                        const OutputPassConfig& config) -> ResultPtr<OutputPass> {
    GOGGLES_PROFILE_FUNCTION();

    auto pass = std::unique_ptr<OutputPass>(new OutputPass());

    pass->m_device = vk_ctx.device;
    pass->m_target_format = config.target_format;
    pass->m_num_sync_indices = config.num_sync_indices;

    GOGGLES_TRY(pass->create_sampler());
    GOGGLES_TRY(pass->create_descriptor_resources());
    GOGGLES_TRY(pass->create_pipeline_layout());
    GOGGLES_TRY(pass->create_pipeline(shader_runtime, config.shader_dir));

    GOGGLES_LOG_DEBUG("OutputPass initialized");
    return make_result_ptr(std::move(pass));
}

void OutputPass::shutdown() {
    if (m_device) {
        if (m_pipeline) {
            m_device.destroyPipeline(m_pipeline);
            m_pipeline = nullptr;
        }
        if (m_pipeline_layout) {
            m_device.destroyPipelineLayout(m_pipeline_layout);
            m_pipeline_layout = nullptr;
        }
        if (m_descriptor_pool) {
            m_device.destroyDescriptorPool(m_descriptor_pool);
            m_descriptor_pool = nullptr;
        }
        if (m_descriptor_layout) {
            m_device.destroyDescriptorSetLayout(m_descriptor_layout);
            m_descriptor_layout = nullptr;
        }
        if (m_sampler) {
            m_device.destroySampler(m_sampler);
            m_sampler = nullptr;
        }
    }
    m_descriptor_sets.clear();
    m_target_format = vk::Format::eUndefined;
    m_device = nullptr;
    m_num_sync_indices = 0;

    GOGGLES_LOG_DEBUG("OutputPass shutdown");
}

void OutputPass::update_descriptor(uint32_t frame_index, vk::ImageView source_view) {
    vk::DescriptorImageInfo image_info{};
    image_info.sampler = m_sampler;
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

void OutputPass::record(vk::CommandBuffer cmd, const PassContext& ctx) {
    GOGGLES_PROFILE_FUNCTION();

    update_descriptor(ctx.frame_index, ctx.source_texture);

    auto scaled = calculate_viewport(ctx.source_extent.width, ctx.source_extent.height,
                                     ctx.output_extent.width, ctx.output_extent.height,
                                     ctx.scale_mode, ctx.integer_scale);

    GOGGLES_LOG_TRACE("OutputPass: source={}x{}, output={}x{}, mode={}, scaled={}x{} @ ({},{})",
                      ctx.source_extent.width, ctx.source_extent.height, ctx.output_extent.width,
                      ctx.output_extent.height, static_cast<int>(ctx.scale_mode), scaled.width,
                      scaled.height, scaled.offset_x, scaled.offset_y);

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
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout, 0,
                           m_descriptor_sets[ctx.frame_index], {});

    vk::Viewport viewport{};
    viewport.x = static_cast<float>(scaled.offset_x);
    viewport.y = static_cast<float>(scaled.offset_y);
    viewport.width = static_cast<float>(scaled.width);
    viewport.height = static_cast<float>(scaled.height);
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

auto OutputPass::create_sampler() -> Result<void> {
    vk::SamplerCreateInfo create_info{};
    create_info.magFilter = vk::Filter::eLinear;
    create_info.minFilter = vk::Filter::eLinear;
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

    auto [result, sampler] = m_device.createSampler(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create sampler: " + vk::to_string(result));
    }

    m_sampler = sampler;
    return {};
}

auto OutputPass::create_descriptor_resources() -> Result<void> {
    vk::DescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    binding.descriptorCount = 1;
    binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo layout_info{};
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;

    auto [layout_result, layout] = m_device.createDescriptorSetLayout(layout_info);
    if (layout_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create descriptor set layout: " +
                                    vk::to_string(layout_result));
    }
    m_descriptor_layout = layout;

    vk::DescriptorPoolSize pool_size{};
    pool_size.type = vk::DescriptorType::eCombinedImageSampler;
    pool_size.descriptorCount = m_num_sync_indices;

    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.maxSets = m_num_sync_indices;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;

    auto [pool_result, pool] = m_device.createDescriptorPool(pool_info);
    if (pool_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create descriptor pool: " + vk::to_string(pool_result));
    }
    m_descriptor_pool = pool;

    std::vector<vk::DescriptorSetLayout> layouts(m_num_sync_indices, m_descriptor_layout);

    vk::DescriptorSetAllocateInfo alloc_info{};
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = m_num_sync_indices;
    alloc_info.pSetLayouts = layouts.data();

    auto [alloc_result, sets] = m_device.allocateDescriptorSets(alloc_info);
    if (alloc_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to allocate descriptor sets: " +
                                    vk::to_string(alloc_result));
    }
    m_descriptor_sets = std::move(sets);

    return {};
}

auto OutputPass::create_pipeline_layout() -> Result<void> {
    vk::PipelineLayoutCreateInfo create_info{};
    create_info.setLayoutCount = 1;
    create_info.pSetLayouts = &m_descriptor_layout;

    auto [result, layout] = m_device.createPipelineLayout(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create pipeline layout: " + vk::to_string(result));
    }

    m_pipeline_layout = layout;
    return {};
}

auto OutputPass::create_pipeline(ShaderRuntime& shader_runtime,
                                 const std::filesystem::path& shader_dir) -> Result<void> {
    // Internal shaders - abort on failure since they're bundled with the app
    auto vert_compiled =
        GOGGLES_MUST(shader_runtime.compile_shader(shader_dir / "internal/blit.vert.slang"));
    auto frag_compiled =
        GOGGLES_MUST(shader_runtime.compile_shader(shader_dir / "internal/blit.frag.slang"));

    vk::ShaderModuleCreateInfo vert_module_info{};
    vert_module_info.codeSize = vert_compiled.spirv.size() * sizeof(uint32_t);
    vert_module_info.pCode = vert_compiled.spirv.data();

    auto [vert_mod_result, vert_module] = m_device.createShaderModule(vert_module_info);
    if (vert_mod_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create vertex shader module: " +
                                    vk::to_string(vert_mod_result));
    }

    vk::ShaderModuleCreateInfo frag_module_info{};
    frag_module_info.codeSize = frag_compiled.spirv.size() * sizeof(uint32_t);
    frag_module_info.pCode = frag_compiled.spirv.data();

    auto [frag_mod_result, frag_module] = m_device.createShaderModule(frag_module_info);
    if (frag_mod_result != vk::Result::eSuccess) {
        m_device.destroyShaderModule(vert_module);
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create fragment shader module: " +
                                    vk::to_string(frag_mod_result));
    }

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = vert_module;
    stages[0].pName = "main";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = frag_module;
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
    create_info.layout = m_pipeline_layout;

    auto [result, pipelines] = m_device.createGraphicsPipelines(nullptr, create_info);
    m_device.destroyShaderModule(frag_module);
    m_device.destroyShaderModule(vert_module);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create graphics pipeline: " + vk::to_string(result));
    }

    m_pipeline = pipelines[0];
    return {};
}

} // namespace goggles::render
