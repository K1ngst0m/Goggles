#include "slang_reflect.hpp"

#include <algorithm>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render {

namespace {

auto extract_members(slang::TypeLayoutReflection* type_layout, size_t base_offset)
    -> std::vector<UniformMember> {
    std::vector<UniformMember> members;

    if (type_layout == nullptr) {
        return members;
    }

    auto field_count = type_layout->getFieldCount();
    for (unsigned i = 0; i < field_count; ++i) {
        auto field = type_layout->getFieldByIndex(i);
        if (field == nullptr) {
            continue;
        }

        UniformMember member;
        member.name = field->getName() != nullptr ? field->getName() : "";
        member.offset = base_offset + field->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM);
        member.size = field->getTypeLayout()->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);

        members.push_back(std::move(member));
    }

    return members;
}

auto slang_type_to_vk_format(slang::TypeReflection* type) -> vk::Format {
    if (type == nullptr) {
        return vk::Format::eUndefined;
    }

    auto kind = type->getKind();
    if (kind == slang::TypeReflection::Kind::Vector) {
        auto element_type = type->getElementType();
        auto element_count = type->getElementCount();

        if (element_type == nullptr) {
            return vk::Format::eUndefined;
        }

        auto scalar_type = element_type->getScalarType();
        if (scalar_type == slang::TypeReflection::ScalarType::Float32) {
            switch (element_count) {
            case 2:
                return vk::Format::eR32G32Sfloat;
            case 3:
                return vk::Format::eR32G32B32Sfloat;
            case 4:
                return vk::Format::eR32G32B32A32Sfloat;
            default:
                return vk::Format::eUndefined;
            }
        }
    } else if (kind == slang::TypeReflection::Kind::Scalar) {
        auto scalar_type = type->getScalarType();
        if (scalar_type == slang::TypeReflection::ScalarType::Float32) {
            return vk::Format::eR32Sfloat;
        }
    }

    return vk::Format::eUndefined;
}

auto get_format_size(vk::Format format) -> uint32_t {
    switch (format) {
    case vk::Format::eR32Sfloat:
        return 4;
    case vk::Format::eR32G32Sfloat:
        return 8;
    case vk::Format::eR32G32B32Sfloat:
        return 12;
    case vk::Format::eR32G32B32A32Sfloat:
        return 16;
    default:
        return 0;
    }
}

void reflect_global_parameters(slang::ProgramLayout* layout, ReflectionData& data) {
    GOGGLES_PROFILE_FUNCTION();
    auto param_count = layout->getParameterCount();
    GOGGLES_LOG_DEBUG("Reflecting {} global parameters", param_count);

    for (unsigned i = 0; i < param_count; ++i) {
        auto param = layout->getParameterByIndex(i);
        if (param == nullptr) {
            continue;
        }

        const char* name = param->getName();
        auto type_layout = param->getTypeLayout();

        if (type_layout == nullptr) {
            continue;
        }

        auto kind = type_layout->getKind();
        auto category = param->getCategory();

        GOGGLES_LOG_TRACE("Parameter {}: name='{}', kind={}, category={}", i,
                          name ? name : "(null)", static_cast<int>(kind),
                          static_cast<int>(category));

        if (category == slang::ParameterCategory::PushConstantBuffer) {
            PushConstantLayout push;
            push.stage_flags =
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

            auto element_type = type_layout->getElementTypeLayout();
            if (element_type != nullptr) {
                push.total_size = element_type->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
                push.members = extract_members(element_type, 0);
            } else {
                push.total_size = type_layout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
                push.members = extract_members(type_layout, 0);
            }

            GOGGLES_LOG_TRACE("Found push constant block: size={}, members={}", push.total_size,
                              push.members.size());
            data.push_constants = std::move(push);
        } else if (category == slang::ParameterCategory::DescriptorTableSlot) {
            auto binding = param->getBindingIndex();
            auto set = param->getBindingSpace();

            if (kind == slang::TypeReflection::Kind::ConstantBuffer ||
                kind == slang::TypeReflection::Kind::ParameterBlock) {
                UniformBufferLayout ubo;
                ubo.binding = binding;
                ubo.set = set;
                ubo.stage_flags =
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

                auto element_type = type_layout->getElementTypeLayout();
                if (element_type != nullptr) {
                    ubo.total_size = element_type->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
                    ubo.members = extract_members(element_type, 0);
                } else {
                    ubo.total_size = type_layout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
                    ubo.members = extract_members(type_layout, 0);
                }

                GOGGLES_LOG_TRACE("Found UBO: binding={}, set={}, size={}, members={}", binding,
                                  set, ubo.total_size, ubo.members.size());
                data.ubo = std::move(ubo);
            } else if (kind == slang::TypeReflection::Kind::Resource ||
                       kind == slang::TypeReflection::Kind::SamplerState ||
                       kind == slang::TypeReflection::Kind::TextureBuffer) {
                TextureBinding tex;
                tex.name = name != nullptr ? name : "";
                tex.binding = binding;
                tex.set = set;
                tex.stage_flags = vk::ShaderStageFlagBits::eFragment;

                GOGGLES_LOG_TRACE("Found texture: name='{}', binding={}, set={}", tex.name, binding,
                                  set);
                data.textures.push_back(std::move(tex));
            }
        } else if (category == slang::ParameterCategory::Uniform) {
            GOGGLES_LOG_TRACE("Found direct uniform: name='{}'", name ? name : "(null)");
        }
    }
}

void reflect_entry_points(slang::ProgramLayout* layout, ReflectionData& data) {
    GOGGLES_PROFILE_FUNCTION();
    auto entry_point_count = layout->getEntryPointCount();
    for (unsigned ep = 0; ep < entry_point_count; ++ep) {
        auto entry_layout = layout->getEntryPointByIndex(ep);
        if (entry_layout == nullptr) {
            continue;
        }

        auto stage = entry_layout->getStage();
        bool is_vertex = (stage == SLANG_STAGE_VERTEX);

        if (is_vertex) {
            uint32_t offset = 0;
            auto ep_param_count = entry_layout->getParameterCount();
            for (unsigned j = 0; j < ep_param_count; ++j) {
                auto param = entry_layout->getParameterByIndex(j);
                if (param == nullptr) {
                    continue;
                }

                auto category = param->getCategory();
                if (category == slang::ParameterCategory::VaryingInput) {
                    auto type_layout = param->getTypeLayout();
                    if (type_layout == nullptr) {
                        continue;
                    }

                    const char* name = param->getName();
                    auto semantic_index = param->getSemanticIndex();

                    VertexInput input;
                    input.name = name != nullptr ? name : "";
                    input.location = static_cast<uint32_t>(semantic_index);
                    input.format = slang_type_to_vk_format(type_layout->getType());
                    input.offset = offset;

                    offset += get_format_size(input.format);

                    GOGGLES_LOG_TRACE("Found vertex input: name='{}', location={}, format={}",
                                      input.name, input.location, vk::to_string(input.format));
                    data.vertex_inputs.push_back(std::move(input));
                }
            }

            // RetroArch shaders always use: Position (vec4, loc 0), TexCoord (vec2, loc 1)
            if (data.vertex_inputs.empty() &&
                (data.push_constants.has_value() || data.ubo.has_value())) {
                GOGGLES_LOG_TRACE(
                    "No vertex inputs from reflection, using RetroArch standard layout");
                data.vertex_inputs.push_back({"Position", 0, vk::Format::eR32G32B32A32Sfloat, 0});
                data.vertex_inputs.push_back({"TexCoord", 1, vk::Format::eR32G32Sfloat, 16});
            }
        }

        auto ep_param_count = entry_layout->getParameterCount();
        for (unsigned j = 0; j < ep_param_count; ++j) {
            auto param = entry_layout->getParameterByIndex(j);
            if (param == nullptr) {
                continue;
            }

            auto type_layout = param->getTypeLayout();
            if (type_layout == nullptr) {
                continue;
            }

            auto kind = type_layout->getKind();
            if (kind == slang::TypeReflection::Kind::Resource ||
                kind == slang::TypeReflection::Kind::SamplerState) {
                const char* name = param->getName();
                auto binding = param->getBindingIndex();
                auto set = param->getBindingSpace();

                TextureBinding tex;
                tex.name = name != nullptr ? name : "";
                tex.binding = binding;
                tex.set = set;
                tex.stage_flags = is_vertex ? vk::ShaderStageFlagBits::eVertex
                                            : vk::ShaderStageFlagBits::eFragment;

                GOGGLES_LOG_TRACE("Found entry point texture: name='{}', binding={}, set={}",
                                  tex.name, binding, set);
                data.textures.push_back(std::move(tex));
            }
        }
    }
}

} // namespace

auto reflect_program(slang::IComponentType* linked) -> Result<ReflectionData> {
    GOGGLES_PROFILE_FUNCTION();
    if (linked == nullptr) {
        return make_error<ReflectionData>(ErrorCode::shader_compile_failed,
                                          "Cannot reflect null program");
    }

    ReflectionData data;
    slang::ProgramLayout* layout = linked->getLayout();

    if (layout == nullptr) {
        return make_error<ReflectionData>(ErrorCode::shader_compile_failed,
                                          "Failed to get program layout");
    }

    reflect_global_parameters(layout, data);
    reflect_entry_points(layout, data);

    std::sort(data.vertex_inputs.begin(), data.vertex_inputs.end(),
              [](const VertexInput& a, const VertexInput& b) { return a.location < b.location; });

    return data;
}

auto reflect_stage(slang::IComponentType* linked, vk::ShaderStageFlags stage)
    -> Result<ReflectionData> {
    GOGGLES_PROFILE_FUNCTION();
    auto result = reflect_program(linked);
    if (!result) {
        return result;
    }

    auto& data = result.value();

    if (data.push_constants.has_value()) {
        data.push_constants->stage_flags = stage;
    }

    if (data.ubo.has_value()) {
        data.ubo->stage_flags = stage;
    }

    for (auto& tex : data.textures) {
        tex.stage_flags = stage;
    }

    if (!(stage & vk::ShaderStageFlagBits::eVertex)) {
        data.vertex_inputs.clear();
    }

    return data;
}

auto merge_reflection(const ReflectionData& vertex, const ReflectionData& fragment)
    -> ReflectionData {
    GOGGLES_PROFILE_FUNCTION();
    ReflectionData merged;

    if (vertex.push_constants.has_value() && fragment.push_constants.has_value()) {
        merged.push_constants = vertex.push_constants;
        merged.push_constants->stage_flags =
            vertex.push_constants->stage_flags | fragment.push_constants->stage_flags;
        if (fragment.push_constants->total_size > merged.push_constants->total_size) {
            merged.push_constants->total_size = fragment.push_constants->total_size;
        }
    } else if (vertex.push_constants.has_value()) {
        merged.push_constants = vertex.push_constants;
    } else if (fragment.push_constants.has_value()) {
        merged.push_constants = fragment.push_constants;
    }

    if (vertex.ubo.has_value() && fragment.ubo.has_value() &&
        vertex.ubo->binding == fragment.ubo->binding) {
        merged.ubo = vertex.ubo;
        merged.ubo->stage_flags = vertex.ubo->stage_flags | fragment.ubo->stage_flags;
    } else if (vertex.ubo.has_value()) {
        merged.ubo = vertex.ubo;
    } else if (fragment.ubo.has_value()) {
        merged.ubo = fragment.ubo;
    }

    merged.textures = vertex.textures;
    for (const auto& frag_tex : fragment.textures) {
        auto it =
            std::find_if(merged.textures.begin(), merged.textures.end(),
                         [&](const TextureBinding& t) { return t.binding == frag_tex.binding; });
        if (it != merged.textures.end()) {
            it->stage_flags |= frag_tex.stage_flags;
        } else {
            merged.textures.push_back(frag_tex);
        }
    }

    merged.vertex_inputs = vertex.vertex_inputs;

    return merged;
}

} // namespace goggles::render
