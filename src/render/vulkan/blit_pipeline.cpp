#include "blit_pipeline.hpp"

#include <util/logging.hpp>

#include <array>

namespace goggles::render {

BlitPipeline::~BlitPipeline() {
    shutdown();
}

auto BlitPipeline::init(vk::Device device,
                        vk::Format swapchain_format,
                        vk::Extent2D swapchain_extent,
                        const std::vector<vk::ImageView>& swapchain_views,
                        pipeline::ShaderRuntime& shader_runtime,
                        const std::filesystem::path& shader_dir) -> Result<void> {
    if (m_initialized) {
        return {};
    }

    m_device = device;

    auto result = create_render_pass(swapchain_format);
    if (!result) return result;

    result = create_sampler();
    if (!result) return result;

    result = create_descriptor_resources();
    if (!result) return result;

    result = create_pipeline_layout();
    if (!result) return result;

    result = create_pipeline(shader_runtime, shader_dir);
    if (!result) return result;

    result = create_framebuffers(swapchain_extent, swapchain_views);
    if (!result) return result;

    m_initialized = true;
    GOGGLES_LOG_DEBUG("BlitPipeline initialized");
    return {};
}

void BlitPipeline::shutdown() {
    if (!m_initialized) {
        return;
    }

    m_framebuffers.clear();
    m_pipeline.reset();
    m_pipeline_layout.reset();
    m_descriptor_pool.reset();
    m_descriptor_layout.reset();
    m_sampler.reset();
    m_render_pass.reset();
    m_device = nullptr;

    m_initialized = false;
    GOGGLES_LOG_DEBUG("BlitPipeline shutdown");
}

void BlitPipeline::recreate_framebuffers(vk::Extent2D swapchain_extent,
                                         const std::vector<vk::ImageView>& swapchain_views) {
    m_framebuffers.clear();
    auto result = create_framebuffers(swapchain_extent, swapchain_views);
    if (!result) {
        GOGGLES_LOG_ERROR("Failed to recreate framebuffers: {}", result.error().message);
    }
}

void BlitPipeline::update_descriptor(vk::ImageView source_view) {
    vk::DescriptorImageInfo image_info{};
    image_info.sampler = *m_sampler;
    image_info.imageView = source_view;
    image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::WriteDescriptorSet write{};
    write.dstSet = m_descriptor_set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo = &image_info;

    m_device.updateDescriptorSets(write, {});
}

void BlitPipeline::record_commands(vk::CommandBuffer cmd,
                                   uint32_t framebuffer_index,
                                   vk::Extent2D extent) {
    vk::RenderPassBeginInfo begin_info{};
    begin_info.renderPass = *m_render_pass;
    begin_info.framebuffer = *m_framebuffers[framebuffer_index];
    begin_info.renderArea.offset = vk::Offset2D{0, 0};
    begin_info.renderArea.extent = extent;

    std::array<vk::ClearValue, 1> clear_values{};
    clear_values[0].color = vk::ClearColorValue{std::array{0.0F, 0.0F, 0.0F, 1.0F}};
    begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    begin_info.pClearValues = clear_values.data();

    cmd.beginRenderPass(begin_info, vk::SubpassContents::eInline);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipeline_layout, 0,
                           m_descriptor_set, {});

    vk::Viewport viewport{};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    cmd.setViewport(0, viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = extent;
    cmd.setScissor(0, scissor);

    cmd.draw(3, 1, 0, 0);
    cmd.endRenderPass();
}

auto BlitPipeline::create_render_pass(vk::Format format) -> Result<void> {
    vk::AttachmentDescription color_attachment{};
    color_attachment.format = format;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    vk::SubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlagBits::eNone;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo create_info{};
    create_info.attachmentCount = 1;
    create_info.pAttachments = &color_attachment;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    auto [result, render_pass] = m_device.createRenderPassUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create render pass: " + vk::to_string(result));
    }

    m_render_pass = std::move(render_pass);
    return {};
}

auto BlitPipeline::create_sampler() -> Result<void> {
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

    auto [result, sampler] = m_device.createSamplerUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create sampler: " + vk::to_string(result));
    }

    m_sampler = std::move(sampler);
    return {};
}

auto BlitPipeline::create_descriptor_resources() -> Result<void> {
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
                                "Failed to create descriptor set layout: " + vk::to_string(layout_result));
    }
    m_descriptor_layout = std::move(layout);

    vk::DescriptorPoolSize pool_size{};
    pool_size.type = vk::DescriptorType::eCombinedImageSampler;
    pool_size.descriptorCount = 1;

    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;

    auto [pool_result, pool] = m_device.createDescriptorPoolUnique(pool_info);
    if (pool_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create descriptor pool: " + vk::to_string(pool_result));
    }
    m_descriptor_pool = std::move(pool);

    vk::DescriptorSetAllocateInfo alloc_info{};
    alloc_info.descriptorPool = *m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &*m_descriptor_layout;

    auto [alloc_result, sets] = m_device.allocateDescriptorSets(alloc_info);
    if (alloc_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to allocate descriptor set: " + vk::to_string(alloc_result));
    }
    m_descriptor_set = sets[0];

    return {};
}

auto BlitPipeline::create_pipeline_layout() -> Result<void> {
    vk::PipelineLayoutCreateInfo create_info{};
    create_info.setLayoutCount = 1;
    create_info.pSetLayouts = &*m_descriptor_layout;

    auto [result, layout] = m_device.createPipelineLayoutUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create pipeline layout: " + vk::to_string(result));
    }

    m_pipeline_layout = std::move(layout);
    return {};
}

auto BlitPipeline::create_pipeline(pipeline::ShaderRuntime& shader_runtime,
                                   const std::filesystem::path& shader_dir) -> Result<void> {
    auto vert_result = shader_runtime.compile_shader(shader_dir / "blit.vert.slang");
    if (!vert_result) {
        return nonstd::make_unexpected(vert_result.error());
    }

    auto frag_result = shader_runtime.compile_shader(shader_dir / "blit.frag.slang");
    if (!frag_result) {
        return nonstd::make_unexpected(frag_result.error());
    }

    vk::ShaderModuleCreateInfo vert_module_info{};
    vert_module_info.codeSize = vert_result->spirv.size() * sizeof(uint32_t);
    vert_module_info.pCode = vert_result->spirv.data();

    auto [vert_mod_result, vert_module] = m_device.createShaderModuleUnique(vert_module_info);
    if (vert_mod_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create vertex shader module: " + vk::to_string(vert_mod_result));
    }

    vk::ShaderModuleCreateInfo frag_module_info{};
    frag_module_info.codeSize = frag_result->spirv.size() * sizeof(uint32_t);
    frag_module_info.pCode = frag_result->spirv.data();

    auto [frag_mod_result, frag_module] = m_device.createShaderModuleUnique(frag_module_info);
    if (frag_mod_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create fragment shader module: " + vk::to_string(frag_mod_result));
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

    vk::GraphicsPipelineCreateInfo create_info{};
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
    create_info.renderPass = *m_render_pass;
    create_info.subpass = 0;

    auto [result, pipelines] = m_device.createGraphicsPipelinesUnique(nullptr, create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create graphics pipeline: " + vk::to_string(result));
    }

    m_pipeline = std::move(pipelines[0]);
    return {};
}

auto BlitPipeline::create_framebuffers(vk::Extent2D extent,
                                       const std::vector<vk::ImageView>& views) -> Result<void> {
    m_framebuffers.reserve(views.size());

    for (const auto& view : views) {
        vk::FramebufferCreateInfo create_info{};
        create_info.renderPass = *m_render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = &view;
        create_info.width = extent.width;
        create_info.height = extent.height;
        create_info.layers = 1;

        auto [result, framebuffer] = m_device.createFramebufferUnique(create_info);
        if (result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                    "Failed to create framebuffer: " + vk::to_string(result));
        }
        m_framebuffers.push_back(std::move(framebuffer));
    }

    return {};
}

} // namespace goggles::render
