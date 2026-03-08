#include <vulkan/vulkan.hpp>

namespace goggles::render {

struct UploadState {
    vk::UniqueImageView image_view;
};

} // namespace goggles::render
