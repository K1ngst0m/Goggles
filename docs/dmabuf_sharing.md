# DMA-BUF Cross-Process Sharing with DRM Format Modifiers

> **Components:** Capture Layer (vk_capture), Render Backend (VulkanBackend), IPC (CaptureReceiver)

## Executive Summary

This document explains how Goggles captures Vulkan application frames and shares them across processes using Linux DMA-BUF with DRM format modifiers. The implementation enables zero-copy GPU texture sharing between the captured application and the Goggles viewer.

**Key Technologies:**
- **DMA-BUF:** Linux kernel mechanism for sharing GPU buffers between processes
- **DRM Format Modifiers:** Metadata describing GPU-specific memory layouts (tiling, compression)
- **Vulkan Extensions:** `VK_EXT_image_drm_format_modifier`, `VK_EXT_external_memory_dma_buf`

---

## 1. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Target Application                               │
│  ┌─────────────┐                                                        │
│  │  Swapchain  │ ◄── Application renders frames here                    │
│  │   Images    │                                                        │
│  └──────┬──────┘                                                        │
│         │ vkQueuePresentKHR (intercepted)                               │
│         ▼                                                               │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Capture Layer (vk_capture)                    │   │
│  │  ┌────────────┐    ┌────────────┐    ┌────────────────────────┐ │   │
│  │  │  Export    │    │  Copy to   │    │  Send via Unix Socket  │ │   │
│  │  │  Image     │◄───│  Export    │───►│  (SCM_RIGHTS)          │ │   │
│  │  │  (DMA-BUF) │    │  Image     │    │  fd + metadata         │ │   │
│  │  └────────────┘    └────────────┘    └────────────────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    │ Unix Domain Socket
                                    │ (DMA-BUF fd via SCM_RIGHTS)
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         Goggles Viewer                                   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    CaptureReceiver                               │   │
│  │  Receives fd + format + stride + modifier                        │   │
│  └──────────────────────────┬──────────────────────────────────────┘   │
│                              │                                          │
│                              ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    VulkanBackend (import_dmabuf)                 │   │
│  │  ┌────────────┐    ┌────────────┐    ┌────────────────────────┐ │   │
│  │  │  Import    │    │  Create    │    │  Sample in             │ │   │
│  │  │  Memory    │───►│  VkImage   │───►│  Fragment Shader       │ │   │
│  │  │  (DMA-BUF) │    │            │    │                        │ │   │
│  │  └────────────┘    └────────────┘    └────────────────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Why DRM Format Modifiers?

### 2.1 The Problem

GPU memory is not always laid out linearly. Modern GPUs use various tiling patterns and compression schemes for performance:

- **Linear:** Simple row-major layout (`rowPitch * height` bytes)
- **Tiled:** GPU-specific tile patterns (e.g., AMD's 64KB tiles, Intel's Y-tiling)
- **Compressed:** Delta Color Compression (DCC), etc.

When sharing GPU buffers across processes, both sides must agree on the memory layout. Without this agreement:
- The importer may interpret data incorrectly
- Vulkan validation layer reports errors
- Visual corruption or crashes occur

### 2.2 The Solution: DRM Format Modifiers

DRM format modifiers are 64-bit values that encode the exact memory layout:

```
Modifier Value          Meaning
─────────────────────────────────────────────────────────
0x0                     DRM_FORMAT_MOD_LINEAR (universal)
0x200000020801b04       AMD vendor-specific tiling
0x100000000000001       Intel Y-tiling
```

By exchanging the modifier value via IPC, both processes can create compatible images.

---

## 3. Implementation Details

### 3.1 Export Side (Capture Layer)

**File:** `src/capture/vk_layer/vk_capture.cpp`

#### Step 1: Query Supported Modifiers

```cpp
// Query which modifiers the driver supports for this format
static std::vector<ModifierInfo> query_export_modifiers(
    VkPhysicalDevice phys_device, VkFormat format, VkInstFuncs* inst_funcs) {

    // Use VkDrmFormatModifierPropertiesListEXT to get available modifiers
    VkDrmFormatModifierPropertiesListEXT modifier_list{};
    modifier_list.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;

    VkFormatProperties2 format_props{};
    format_props.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    format_props.pNext = &modifier_list;

    // First call: get count
    inst_funcs->GetPhysicalDeviceFormatProperties2(phys_device, format, &format_props);

    // Second call: get actual modifiers
    std::vector<VkDrmFormatModifierPropertiesEXT> modifiers(modifier_list.drmFormatModifierCount);
    modifier_list.pDrmFormatModifierProperties = modifiers.data();
    inst_funcs->GetPhysicalDeviceFormatProperties2(phys_device, format, &format_props);

    // Filter: only single-plane modifiers supporting TRANSFER_DST
    // (we need TRANSFER_DST to copy swapchain image to export image)
}
```

#### Step 2: Create Export Image (List-Based)

**Key Insight:** We use `VkImageDrmFormatModifierListCreateInfoEXT` (not Explicit) because we don't know the optimal layout upfront. The driver selects the best modifier from our list.

```cpp
// Provide list of acceptable modifiers
VkImageDrmFormatModifierListCreateInfoEXT modifier_list_info{};
modifier_list_info.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
modifier_list_info.drmFormatModifierCount = static_cast<uint32_t>(modifier_list.size());
modifier_list_info.pDrmFormatModifiers = modifier_list.data();

// Chain: image_info → ext_mem_info → modifier_list_info
VkExternalMemoryImageCreateInfo ext_mem_info{};
ext_mem_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
ext_mem_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
ext_mem_info.pNext = &modifier_list_info;

VkImageCreateInfo image_info{};
image_info.pNext = &ext_mem_info;
image_info.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // Copy destination
// ...

vkCreateImage(device, &image_info, nullptr, &export_image);
```

#### Step 3: Query Actual Selected Modifier

After image creation, the driver has selected a modifier. We must query it to send via IPC:

```cpp
VkImageDrmFormatModifierPropertiesEXT modifier_props{};
modifier_props.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT;

vkGetImageDrmFormatModifierPropertiesEXT(device, export_image, &modifier_props);
swap->dmabuf_modifier = modifier_props.drmFormatModifier;
```

#### Step 4: Export DMA-BUF File Descriptor

```cpp
VkMemoryGetFdInfoKHR fd_info{};
fd_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
fd_info.memory = export_memory;
fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

int dmabuf_fd;
vkGetMemoryFdKHR(device, &fd_info, &dmabuf_fd);
```

#### Step 5: Send via IPC

The capture layer sends the following via Unix socket with `SCM_RIGHTS`:
- `dmabuf_fd` - File descriptor (passed via SCM_RIGHTS)
- `width`, `height` - Image dimensions
- `format` - Vulkan format (e.g., `VK_FORMAT_A2R10G10B10_UNORM_PACK32`)
- `stride` - Row pitch in bytes
- `modifier` - DRM format modifier value

---

### 3.2 Import Side (Render Backend)

**File:** `src/render/backend/vulkan_backend.cpp`

#### Step 1: Receive IPC Message

**File:** `src/capture/capture_receiver.cpp`

```cpp
struct CaptureFrame {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;
    uint64_t modifier;  // DRM format modifier from exporter
    int dmabuf_fd;      // File descriptor received via SCM_RIGHTS
};
```

#### Step 2: Create Import Image (Explicit Modifier)

**Key Insight:** On import, we use
`VkImageDrmFormatModifierExplicitCreateInfoEXT` because we know the exact
modifier and stride from the exporter.

```cpp
// Set up plane layout with stride from IPC
vk::SubresourceLayout plane_layout{};
plane_layout.offset = 0;
plane_layout.rowPitch = frame.stride;  // From exporter
plane_layout.size = 0;  // Determined by driver
plane_layout.arrayPitch = 0;
plane_layout.depthPitch = 0;

// Explicit modifier creation - we know exactly what the exporter used
vk::ImageDrmFormatModifierExplicitCreateInfoEXT modifier_info{};
modifier_info.drmFormatModifier = frame.modifier;  // From exporter
modifier_info.drmFormatModifierPlaneCount = 1;
modifier_info.pPlaneLayouts = &plane_layout;

// Chain: image_info → ext_mem_info → modifier_info
vk::ExternalMemoryImageCreateInfo ext_mem_info{};
ext_mem_info.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT;
ext_mem_info.pNext = &modifier_info;

vk::ImageCreateInfo image_info{};
image_info.pNext = &ext_mem_info;
image_info.tiling = vk::ImageTiling::eDrmFormatModifierEXT;
image_info.usage = vk::ImageUsageFlagBits::eSampled;  // For shader sampling
// ...
```

#### Step 3: Import DMA-BUF Memory with Dedicated Allocation

**Key Insight:** Many vendor-specific modifiers require dedicated memory allocation. We must query this requirement.

```cpp
// Query memory requirements including dedicated allocation needs
vk::ImageMemoryRequirementsInfo2 mem_reqs_info{};
mem_reqs_info.image = imported_image;

vk::MemoryDedicatedRequirements dedicated_reqs{};
vk::MemoryRequirements2 mem_reqs2{};
mem_reqs2.pNext = &dedicated_reqs;

device.getImageMemoryRequirements2(&mem_reqs_info, &mem_reqs2);

// Set up DMA-BUF import
vk::ImportMemoryFdInfoKHR import_info{};
import_info.handleType = vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT;
import_info.fd = dup(frame.dmabuf_fd);  // Vulkan takes ownership

// Add dedicated allocation if required
vk::MemoryDedicatedAllocateInfo dedicated_alloc{};
dedicated_alloc.image = imported_image;

if (dedicated_reqs.requiresDedicatedAllocation || dedicated_reqs.prefersDedicatedAllocation) {
    import_info.pNext = &dedicated_alloc;
}

vk::MemoryAllocateInfo alloc_info{};
alloc_info.pNext = &import_info;
alloc_info.allocationSize = mem_reqs2.memoryRequirements.size;
// ...

device.allocateMemory(alloc_info);
device.bindImageMemory(imported_image, imported_memory, 0);
```

---

## 4. Required Vulkan Extensions

### Export Side (Capture Layer)

| Extension | Purpose |
|-----------|---------|
| `VK_EXT_image_drm_format_modifier` | Create images with modifier tiling |
| `VK_KHR_external_memory_fd` | Export memory as file descriptor |
| `VK_EXT_external_memory_dma_buf` | DMA-BUF handle type support |

### Import Side (Render Backend)

| Extension | Purpose |
|-----------|---------|
| `VK_EXT_image_drm_format_modifier` | Create images with modifier tiling |
| `VK_KHR_image_format_list` | Required dependency for modifier ext |
| `VK_KHR_external_memory_fd` | Import memory from file descriptor |
| `VK_EXT_external_memory_dma_buf` | DMA-BUF handle type support |

---

## 5. Common Issues and Solutions

### 5.1 VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT

**Cause:** Using explicit modifier creation (`VkImageDrmFormatModifierExplicitCreateInfoEXT`) with invalid plane layout (e.g., `rowPitch = 0`).

**Solution:**
- On **export**: Use list-based creation (`VkImageDrmFormatModifierListCreateInfoEXT`) and let driver determine layout
- On **import**: Use explicit creation with correct stride from IPC

### 5.2 requiresDedicatedAllocation Validation Error

**Cause:** Images with vendor-specific modifiers often require dedicated memory allocation, but standard allocation was used.

**Solution:** Query `VkMemoryDedicatedRequirements` via `vkGetImageMemoryRequirements2` and add `VkMemoryDedicatedAllocateInfo` when required.

### 5.3 Format Doesn't Support LINEAR Tiling

**Cause:** Some formats (e.g., `A2R10G10B10_UNORM_PACK32`) don't support LINEAR tiling with certain usage flags.

**Solution:** Use DRM format modifiers instead of hardcoded LINEAR tiling.

---

## 6. Data Flow Summary

```
┌─────────────────────────────────────────────────────────────────────────┐
│                            EXPORT FLOW                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1. Query modifiers: vkGetPhysicalDeviceFormatProperties2               │
│         │                                                               │
│         ▼                                                               │
│  2. Create image with VkImageDrmFormatModifierListCreateInfoEXT         │
│     (tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)                  │
│         │                                                               │
│         ▼                                                               │
│  3. Driver selects optimal modifier                                     │
│         │                                                               │
│         ▼                                                               │
│  4. Query actual modifier: vkGetImageDrmFormatModifierPropertiesEXT     │
│         │                                                               │
│         ▼                                                               │
│  5. Allocate exportable memory (VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF) │
│         │                                                               │
│         ▼                                                               │
│  6. Export fd: vkGetMemoryFdKHR                                         │
│         │                                                               │
│         ▼                                                               │
│  7. Send via IPC: {fd, format, width, height, stride, modifier}         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                            IMPORT FLOW                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1. Receive IPC: {fd, format, width, height, stride, modifier}          │
│         │                                                               │
│         ▼                                                               │
│  2. Create image with VkImageDrmFormatModifierExplicitCreateInfoEXT     │
│     (tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)                  │
│     (plane layout with stride from IPC)                                 │
│         │                                                               │
│         ▼                                                               │
│  3. Query dedicated allocation: vkGetImageMemoryRequirements2           │
│         │                                                               │
│         ▼                                                               │
│  4. Import memory: VkImportMemoryFdInfoKHR + VkMemoryDedicatedAllocateInfo│
│         │                                                               │
│         ▼                                                               │
│  5. Bind: vkBindImageMemory                                             │
│         │                                                               │
│         ▼                                                               │
│  6. Create image view and sample in shader                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 7. References

- [Vulkan VK_EXT_image_drm_format_modifier](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_image_drm_format_modifier.html)
- [Linux DRM Format Modifiers](https://docs.kernel.org/gpu/drm-kms.html#format-modifiers)
- [DMA-BUF Sharing](https://docs.kernel.org/driver-api/dma-buf.html)
