#include "slang_reflect.hpp"

#include <util/logging.hpp>

namespace goggles::render {

namespace {

// Helper to extract uniform members from a type layout
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

} // namespace

auto reflect_program(slang::IComponentType* linked) -> Result<ReflectionData> {
    if (linked == nullptr) {
        return make_error<ReflectionData>(ErrorCode::SHADER_COMPILE_FAILED,
                                          "Cannot reflect null program");
    }

    ReflectionData data;
    slang::ProgramLayout* layout = linked->getLayout();

    if (layout == nullptr) {
        return make_error<ReflectionData>(ErrorCode::SHADER_COMPILE_FAILED,
                                          "Failed to get program layout");
    }

    // Iterate over global parameters
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

        GOGGLES_LOG_DEBUG("Parameter {}: name='{}', kind={}, category={}", i, name ? name : "(null)",
                          static_cast<int>(kind), static_cast<int>(category));

        if (category == slang::ParameterCategory::PushConstantBuffer) {
            // Push constant block
            PushConstantLayout push;
            push.total_size = type_layout->getSize(SLANG_PARAMETER_CATEGORY_PUSH_CONSTANT_BUFFER);
            push.members = extract_members(type_layout, 0);

            GOGGLES_LOG_DEBUG("Found push constant block: size={}, members={}", push.total_size,
                              push.members.size());
            data.push_constants = std::move(push);
        } else if (category == slang::ParameterCategory::DescriptorTableSlot) {
            // Could be UBO or texture
            auto binding = param->getBindingIndex();
            auto set = param->getBindingSpace();

            if (kind == slang::TypeReflection::Kind::ConstantBuffer ||
                kind == slang::TypeReflection::Kind::ParameterBlock) {
                // Uniform buffer
                UniformBufferLayout ubo;
                ubo.binding = binding;
                ubo.set = set;

                // Get element type for constant buffer
                auto element_type = type_layout->getElementTypeLayout();
                if (element_type != nullptr) {
                    ubo.total_size = element_type->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
                    ubo.members = extract_members(element_type, 0);
                } else {
                    ubo.total_size = type_layout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
                    ubo.members = extract_members(type_layout, 0);
                }

                GOGGLES_LOG_DEBUG("Found UBO: binding={}, set={}, size={}, members={}", binding, set,
                                  ubo.total_size, ubo.members.size());
                data.ubo = std::move(ubo);
            } else if (kind == slang::TypeReflection::Kind::Resource ||
                       kind == slang::TypeReflection::Kind::SamplerState ||
                       kind == slang::TypeReflection::Kind::TextureBuffer) {
                // Texture or sampler
                TextureBinding tex;
                tex.name = name != nullptr ? name : "";
                tex.binding = binding;
                tex.set = set;

                GOGGLES_LOG_DEBUG("Found texture: name='{}', binding={}, set={}", tex.name, binding,
                                  set);
                data.textures.push_back(std::move(tex));
            }
        } else if (category == slang::ParameterCategory::Uniform) {
            // Direct uniform in default constant buffer
            GOGGLES_LOG_DEBUG("Found direct uniform: name='{}'", name ? name : "(null)");
        }
    }

    // Also check entry point parameters for textures/samplers
    auto entry_point_count = layout->getEntryPointCount();
    for (unsigned ep = 0; ep < entry_point_count; ++ep) {
        auto entry_layout = layout->getEntryPointByIndex(ep);
        if (entry_layout == nullptr) {
            continue;
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

                GOGGLES_LOG_DEBUG("Found entry point texture: name='{}', binding={}, set={}",
                                  tex.name, binding, set);
                data.textures.push_back(std::move(tex));
            }
        }
    }

    return data;
}

} // namespace goggles::render
