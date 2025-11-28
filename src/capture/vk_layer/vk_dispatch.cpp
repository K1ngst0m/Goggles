// Vulkan layer object tracking implementation

#include "vk_dispatch.hpp"

#include <cstdio>
#define LAYER_DEBUG(fmt, ...) fprintf(stderr, "[goggles-layer] " fmt "\n", ##__VA_ARGS__)

namespace goggles::capture {

// =============================================================================
// ObjectTracker Implementation
// =============================================================================

void ObjectTracker::add_instance(VkInstance instance, VkInstData data) {
    std::lock_guard lock(mutex_);
    instances_[GET_LDT(instance)] = data;
}

VkInstData* ObjectTracker::get_instance(VkInstance instance) {
    std::lock_guard lock(mutex_);
    auto it = instances_.find(GET_LDT(instance));
    return it != instances_.end() ? &it->second : nullptr;
}

VkInstData* ObjectTracker::get_instance_by_physical_device(VkPhysicalDevice device) {
    std::lock_guard lock(mutex_);
    auto phys_it = phys_to_instance_.find(GET_LDT(device));
    if (phys_it == phys_to_instance_.end()) {
        return nullptr;
    }
    auto inst_it = instances_.find(GET_LDT(phys_it->second));
    return inst_it != instances_.end() ? &inst_it->second : nullptr;
}

void ObjectTracker::remove_instance(VkInstance instance) {
    std::lock_guard lock(mutex_);
    instances_.erase(GET_LDT(instance));
}

void ObjectTracker::add_device(VkDevice device, VkDeviceData data) {
    std::lock_guard lock(mutex_);
    devices_[GET_LDT(device)] = data;
}

VkDeviceData* ObjectTracker::get_device(VkDevice device) {
    std::lock_guard lock(mutex_);
    auto it = devices_.find(GET_LDT(device));
    return it != devices_.end() ? &it->second : nullptr;
}

VkDeviceData* ObjectTracker::get_device_by_queue(VkQueue queue) {
    std::lock_guard lock(mutex_);
    // VkQueue shares the same dispatch table as VkDevice at this layer level,
    // so we can directly look up using the queue's LDT
    void* ldt = GET_LDT(queue);
    auto dev_it = devices_.find(ldt);
    if (dev_it != devices_.end()) {
        return &dev_it->second;
    }
    // Fallback for single-device case
    if (!devices_.empty()) {
        return &devices_.begin()->second;
    }
    LAYER_DEBUG("get_device_by_queue: no devices found");
    return nullptr;
}

void ObjectTracker::remove_device(VkDevice device) {
    std::lock_guard lock(mutex_);
    devices_.erase(GET_LDT(device));
}

void ObjectTracker::add_queue(VkQueue queue, VkDevice device) {
    std::lock_guard lock(mutex_);
    void* queue_ldt = GET_LDT(queue);
    queue_to_device_[queue_ldt] = device;
    LAYER_DEBUG("add_queue: queue_ldt=%p -> device=%p", queue_ldt, (void*)device);
}

void ObjectTracker::remove_queues_for_device(VkDevice device) {
    std::lock_guard lock(mutex_);
    for (auto it = queue_to_device_.begin(); it != queue_to_device_.end();) {
        if (it->second == device) {
            it = queue_to_device_.erase(it);
        } else {
            ++it;
        }
    }
}

void ObjectTracker::add_physical_device(VkPhysicalDevice phys, VkInstance inst) {
    std::lock_guard lock(mutex_);
    phys_to_instance_[GET_LDT(phys)] = inst;
}

// Global instance
ObjectTracker& get_object_tracker() {
    static ObjectTracker tracker;
    return tracker;
}

} // namespace goggles::capture
