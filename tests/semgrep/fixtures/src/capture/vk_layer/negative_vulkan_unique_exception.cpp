#include <vulkan/vulkan.hpp>

namespace goggles::capture::vk_layer {

struct LayerUploadState {
    vk::UniqueImageView image_view;
};

} // namespace goggles::capture::vk_layer
