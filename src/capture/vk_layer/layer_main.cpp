#include "vk_dispatch.hpp"
#include "vk_hooks.hpp"

#include <cstring>
#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>

// Define VK_LAYER_EXPORT for symbol visibility
#if defined(__GNUC__) && __GNUC__ >= 4
#define VK_LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(_MSC_VER)
#define VK_LAYER_EXPORT __declspec(dllexport)
#else
#define VK_LAYER_EXPORT
#endif

namespace goggles::capture {

// Must match manifest JSON
static constexpr const char* LAYER_NAME = "VK_LAYER_goggles_capture";

#define GETPROCADDR(func)                                                                          \
    if (strcmp(pName, "vk" #func) == 0) {                                                          \
        return reinterpret_cast<PFN_vkVoidFunction>(&Goggles_##func);                              \
    }

static PFN_vkVoidFunction VKAPI_CALL Goggles_GetDeviceProcAddr(VkDevice device, const char* pName);

static PFN_vkVoidFunction VKAPI_CALL Goggles_GetInstanceProcAddr(VkInstance instance,
                                                                 const char* pName);

static PFN_vkVoidFunction VKAPI_CALL Goggles_GetDeviceProcAddr(VkDevice device, const char* pName) {
    GETPROCADDR(GetDeviceProcAddr);
    GETPROCADDR(DestroyDevice);
    GETPROCADDR(CreateSwapchainKHR);
    GETPROCADDR(DestroySwapchainKHR);
    GETPROCADDR(QueuePresentKHR);

    auto* data = get_object_tracker().get_device(device);
    if (data && data->funcs.GetDeviceProcAddr) {
        return data->funcs.GetDeviceProcAddr(device, pName);
    }
    return nullptr;
}

static PFN_vkVoidFunction VKAPI_CALL Goggles_GetInstanceProcAddr(VkInstance instance,
                                                                 const char* pName) {
    GETPROCADDR(GetInstanceProcAddr);
    GETPROCADDR(CreateInstance);
    GETPROCADDR(DestroyInstance);
    GETPROCADDR(GetDeviceProcAddr);
    GETPROCADDR(CreateDevice);
    GETPROCADDR(DestroyDevice);
    GETPROCADDR(CreateSwapchainKHR);
    GETPROCADDR(DestroySwapchainKHR);
    GETPROCADDR(QueuePresentKHR);

    if (instance) {
        auto* data = get_object_tracker().get_instance(instance);
        if (data && data->funcs.GetInstanceProcAddr) {
            return data->funcs.GetInstanceProcAddr(instance, pName);
        }
    }
    return nullptr;
}

#undef GETPROCADDR

} // namespace goggles::capture

extern "C" {

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct) {
    if (pVersionStruct == nullptr) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (pVersionStruct->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (pVersionStruct->loaderLayerInterfaceVersion >= 2) {
        pVersionStruct->pfnGetInstanceProcAddr = goggles::capture::Goggles_GetInstanceProcAddr;
        pVersionStruct->pfnGetDeviceProcAddr = goggles::capture::Goggles_GetDeviceProcAddr;
        pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;
    }

    if (pVersionStruct->loaderLayerInterfaceVersion > 2) {
        pVersionStruct->loaderLayerInterfaceVersion = 2;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance,
                                                                               const char* pName) {
    return goggles::capture::Goggles_GetInstanceProcAddr(instance, pName);
}

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device,
                                                                             const char* pName) {
    return goggles::capture::Goggles_GetDeviceProcAddr(device, pName);
}

} // extern "C"
