#include "downsample_pass.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <render/shader/shader_runtime.hpp>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render {

namespace {

constexpr uint32_t LINEAR_SOURCE_BINDING = 0;
constexpr uint32_t NEAREST_SOURCE_BINDING = 1;
constexpr std::string_view FILTER_TYPE_PARAMETER_NAME = "filter_type";

/// @brief Push constants for the downsample shader.
struct DownsamplePushConstants {
    float source_width;
    float source_height;
    float source_inv_width;
    float source_inv_height;
    float target_width;
    float target_height;
    float target_inv_width;
    float target_inv_height;
    float filter_type;
};

} // namespace

auto DownsamplePass::shader_parameters(float filter_type) -> std::vector<ShaderParameter> {
    return {{
        .name = std::string(FILTER_TYPE_PARAMETER_NAME),
        .description = "Filter Type",
        .default_value = DownsamplePass::FILTER_TYPE_DEFAULT,
        .current_value = sanitize_parameter_value(FILTER_TYPE_PARAMETER_NAME, filter_type),
        .min_value = 0.0F,
        .max_value = DownsamplePass::FILTER_TYPE_NEAREST,
        .step = 1.0F,
    }};
}

auto DownsamplePass::sanitize_parameter_value(std::string_view name, float value) -> float {
    if (name == FILTER_TYPE_PARAMETER_NAME) {
        return std::round(std::clamp(value, DownsamplePass::FILTER_TYPE_DEFAULT,
                                     DownsamplePass::FILTER_TYPE_NEAREST));
    }
    return value;
}

DownsamplePass::~DownsamplePass() {
    DownsamplePass::shutdown();
}

auto DownsamplePass::create(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                            const DownsamplePassConfig& config) -> ResultPtr<DownsamplePass> {
    GOGGLES_PROFILE_FUNCTION();

    auto pass = std::unique_ptr<DownsamplePass>(new DownsamplePass());

    pass->m_device = vk_ctx.device;
    pass->m_target_format = config.target_format;
    pass->m_num_sync_indices = config.num_sync_indices;

    GOGGLES_TRY(pass->create_samplers());
    GOGGLES_TRY(pass->create_descriptor_resources());
    GOGGLES_TRY(pass->create_pipeline_layout());
    GOGGLES_TRY(pass->create_pipeline(shader_runtime, config.shader_dir));

    GOGGLES_LOG_DEBUG("DownsamplePass initialized");
    return make_result_ptr(std::move(pass));
}

void DownsamplePass::shutdown() {
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
        if (m_nearest_sampler) {
            m_device.destroySampler(m_nearest_sampler);
            m_nearest_sampler = nullptr;
        }
        if (m_linear_sampler) {
            m_device.destroySampler(m_linear_sampler);
            m_linear_sampler = nullptr;
        }
    }
    m_descriptor_sets.clear();
    m_target_format = vk::Format::eUndefined;
    m_device = nullptr;
    m_num_sync_indices = 0;

    GOGGLES_LOG_DEBUG("DownsamplePass shutdown");
}

auto DownsamplePass::get_shader_parameters() const -> std::vector<ShaderParameter> {
    return shader_parameters(m_filter_type);
}

void DownsamplePass::set_shader_parameter(const std::string& name, float value) {
    if (name == "filter_type") {
        m_filter_type = sanitize_parameter_value(name, value);
    }
}

void DownsamplePass::update_descriptor(uint32_t frame_index, vk::ImageView source_view) {
    std::array<vk::DescriptorImageInfo, 2> image_infos{};
    image_infos[0].sampler = m_linear_sampler;
    image_infos[0].imageView = source_view;
    image_infos[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_infos[1].sampler = m_nearest_sampler;
    image_infos[1].imageView = source_view;
    image_infos[1].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    std::array<vk::WriteDescriptorSet, 2> writes{};
    writes[0].dstSet = m_descriptor_sets[frame_index];
    writes[0].dstBinding = LINEAR_SOURCE_BINDING;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[0].pImageInfo = &image_infos[0];
    writes[1].dstSet = m_descriptor_sets[frame_index];
    writes[1].dstBinding = NEAREST_SOURCE_BINDING;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[1].pImageInfo = &image_infos[1];

    m_device.updateDescriptorSets(writes, {});
}

void DownsamplePass::record(vk::CommandBuffer cmd, const PassContext& ctx) {
    GOGGLES_PROFILE_FUNCTION();

    update_descriptor(ctx.frame_index, ctx.source_texture);

    DownsamplePushConstants pc{};
    pc.source_width = static_cast<float>(ctx.source_extent.width);
    pc.source_height = static_cast<float>(ctx.source_extent.height);
    pc.source_inv_width = 1.0F / pc.source_width;
    pc.source_inv_height = 1.0F / pc.source_height;
    pc.target_width = static_cast<float>(ctx.output_extent.width);
    pc.target_height = static_cast<float>(ctx.output_extent.height);
    pc.target_inv_width = 1.0F / pc.target_width;
    pc.target_inv_height = 1.0F / pc.target_height;
    pc.filter_type = m_filter_type;

    GOGGLES_LOG_TRACE("DownsamplePass: source={}x{} -> target={}x{}", ctx.source_extent.width,
                      ctx.source_extent.height, ctx.output_extent.width, ctx.output_extent.height);

    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.imageView = ctx.target_image_view;
    color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_attachment.loadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;

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

    cmd.pushConstants<DownsamplePushConstants>(m_pipeline_layout,
                                               vk::ShaderStageFlagBits::eFragment, 0, pc);

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

auto DownsamplePass::create_samplers() -> Result<void> {
    vk::SamplerCreateInfo create_info{};
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

    create_info.magFilter = vk::Filter::eLinear;
    create_info.minFilter = vk::Filter::eLinear;
    auto [linear_result, linear_sampler] = m_device.createSampler(create_info);
    if (linear_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create linear downsample sampler: " +
                                    vk::to_string(linear_result));
    }
    m_linear_sampler = linear_sampler;

    create_info.magFilter = vk::Filter::eNearest;
    create_info.minFilter = vk::Filter::eNearest;
    auto [nearest_result, nearest_sampler] = m_device.createSampler(create_info);
    if (nearest_result != vk::Result::eSuccess) {
        m_device.destroySampler(m_linear_sampler);
        m_linear_sampler = nullptr;
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create nearest downsample sampler: " +
                                    vk::to_string(nearest_result));
    }
    m_nearest_sampler = nearest_sampler;

    return {};
}

auto DownsamplePass::create_descriptor_resources() -> Result<void> {
    std::array<vk::DescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = LINEAR_SOURCE_BINDING;
    bindings[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = vk::ShaderStageFlagBits::eFragment;
    bindings[1].binding = NEAREST_SOURCE_BINDING;
    bindings[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo layout_info{};
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    auto [layout_result, layout] = m_device.createDescriptorSetLayout(layout_info);
    if (layout_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create descriptor set layout: " +
                                    vk::to_string(layout_result));
    }
    m_descriptor_layout = layout;

    vk::DescriptorPoolSize pool_size{};
    pool_size.type = vk::DescriptorType::eCombinedImageSampler;
    pool_size.descriptorCount = m_num_sync_indices * static_cast<uint32_t>(bindings.size());

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

auto DownsamplePass::create_pipeline_layout() -> Result<void> {
    vk::PushConstantRange push_constant{};
    push_constant.stageFlags = vk::ShaderStageFlagBits::eFragment;
    push_constant.offset = 0;
    push_constant.size = sizeof(DownsamplePushConstants);

    vk::PipelineLayoutCreateInfo create_info{};
    create_info.setLayoutCount = 1;
    create_info.pSetLayouts = &m_descriptor_layout;
    create_info.pushConstantRangeCount = 1;
    create_info.pPushConstantRanges = &push_constant;

    auto [result, layout] = m_device.createPipelineLayout(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create pipeline layout: " + vk::to_string(result));
    }

    m_pipeline_layout = layout;
    return {};
}

auto DownsamplePass::create_pipeline(ShaderRuntime& shader_runtime,
                                     const std::filesystem::path& shader_dir) -> Result<void> {
    // Internal shaders - abort on failure since they're bundled with the app
    auto vert_compiled =
        GOGGLES_MUST(shader_runtime.compile_shader(shader_dir / "internal/blit.vert.slang"));
    auto frag_compiled =
        GOGGLES_MUST(shader_runtime.compile_shader(shader_dir / "internal/downsample.frag.slang"));

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
