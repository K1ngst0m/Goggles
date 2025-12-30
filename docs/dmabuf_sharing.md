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
┌───────────────────────────────────────┐
│         Target Application            │
│  ┌─────────────┐                      │
│  │  Swapchain  │                      │
│  └──────┬──────┘                      │
│         │ vkQueuePresentKHR           │
│         ▼                             │
│  ┌─────────────────────────────────┐  │
│  │  Capture Layer                  │  │
│  │  Export DMA-BUF                 │  │
│  └──────────────┬──────────────────┘  │
└─────────────────┼─────────────────────┘
                  │
                  │ Unix Socket + Semaphore Sync
                  ▼
┌─────────────────┼─────────────────────┐
│  Goggles Viewer │                     │
│  ┌──────────────┴──────────────────┐  │
│  │  CaptureReceiver                │  │
│  └──────────────┬──────────────────┘  │
│                 ▼                     │
│  ┌─────────────────────────────────┐  │
│  │  VulkanBackend                  │  │
│  │  Import DMA-BUF ──► FilterChain │  │
│  └─────────────────────────────────┘  │
└───────────────────────────────────────┘
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
│  7. Create timeline semaphores + export fds: vkGetSemaphoreFdKHR        │
│         │                                                               │
│         ▼                                                               │
│  8. Send via IPC: {fd, format, width, height, stride, modifier}         │
│     + semaphore fds (frame_ready, frame_consumed)                       │
│         │                                                               │
│         ▼                                                               │
│  9. Wait on frame_consumed semaphore (back-pressure)                    │
│         │                                                               │
│         ▼                                                               │
│  10. Signal frame_ready semaphore after copy completes                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                            IMPORT FLOW                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1. Receive IPC: {fd, format, width, height, stride, modifier}          │
│     + semaphore fds (frame_ready, frame_consumed)                       │
│         │                                                               │
│         ▼                                                               │
│  2. Import semaphores: vkImportSemaphoreFdKHR                           │
│         │                                                               │
│         ▼                                                               │
│  3. Create image with VkImageDrmFormatModifierExplicitCreateInfoEXT     │
│     (tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)                  │
│     (plane layout with stride from IPC)                                 │
│         │                                                               │
│         ▼                                                               │
│  4. Query dedicated allocation: vkGetImageMemoryRequirements2           │
│         │                                                               │
│         ▼                                                               │
│  5. Import memory: VkImportMemoryFdInfoKHR + VkMemoryDedicatedAllocateInfo│
│         │                                                               │
│         ▼                                                               │
│  6. Bind: vkBindImageMemory                                             │
│         │                                                               │
│         ▼                                                               │
│  7. Wait on frame_ready semaphore before rendering                      │
│         │                                                               │
│         ▼                                                               │
│  8. Create image view and sample in shader                              │
│         │                                                               │
│         ▼                                                               │
│  9. Signal frame_consumed semaphore after render completes              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Cross-Process Semaphore Synchronization

Timeline semaphores enable GPU-to-GPU synchronization between the capture layer and Goggles viewer without CPU polling.

### 7.1 Why Timeline Semaphores?

Binary semaphores require one-to-one signal/wait pairing. Timeline semaphores use monotonically increasing counter values, enabling:
- **Back-pressure:** Capture waits for receiver to consume previous frame before overwriting
- **Non-blocking checks:** Host can query semaphore value without blocking
- **Cross-process sharing:** Same semaphore instance shared via file descriptor

### 7.2 Semaphore Protocol

Two timeline semaphores coordinate frame handoff:

| Semaphore | Signaled By | Waited On By | Purpose |
|-----------|-------------|--------------|---------|
| `frame_ready` | Capture layer | Goggles viewer | Frame data is ready for sampling |
| `frame_consumed` | Goggles viewer | Capture layer | Frame has been rendered (back-pressure) |

**Synchronization flow:**
```
Capture Layer                          Goggles Viewer
─────────────                          ──────────────
1. Wait frame_consumed[N-1]
2. Copy swapchain → export image
3. Signal frame_ready[N] ─────────────► 4. Wait frame_ready[N]
                                        5. Import + render frame
                          ◄───────────── 6. Signal frame_consumed[N]
```

### 7.3 Export Side Implementation

**File:** `src/capture/vk_layer/vk_capture.cpp`

```cpp
// Create exportable timeline semaphores
VkExportSemaphoreCreateInfo export_info{};
export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

VkSemaphoreTypeCreateInfo timeline_info{};
timeline_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
timeline_info.pNext = &export_info;
timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
timeline_info.initialValue = 0;

VkSemaphoreCreateInfo sem_info{};
sem_info.pNext = &timeline_info;
vkCreateSemaphore(device, &sem_info, nullptr, &frame_ready_sem);

// Export to file descriptor
VkSemaphoreGetFdInfoKHR fd_info{};
fd_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
fd_info.semaphore = frame_ready_sem;
fd_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
vkGetSemaphoreFdKHR(device, &fd_info, &frame_ready_fd);
```

### 7.4 Import Side Implementation

**File:** `src/render/backend/vulkan_backend.cpp`

```cpp
// Create local timeline semaphore
vk::SemaphoreTypeCreateInfo timeline_info{};
timeline_info.semaphoreType = vk::SemaphoreType::eTimeline;
timeline_info.initialValue = 0;

vk::SemaphoreCreateInfo sem_info{};
sem_info.pNext = &timeline_info;
auto [res, ready_sem] = device.createSemaphore(sem_info);

// Import the file descriptor
VkImportSemaphoreFdInfoKHR import_info{};
import_info.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
import_info.semaphore = ready_sem;
import_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
import_info.fd = frame_ready_fd;  // Vulkan takes ownership
vkImportSemaphoreFdKHR(device, &import_info);
```

### 7.5 IPC Protocol

Semaphores are sent via a dedicated message type:

```cpp
enum class CaptureMessageType : uint32_t {
    // ...
    semaphore_init = 4,  // Semaphore export message
};

struct CaptureSemaphoreInit {
    CaptureMessageType type = CaptureMessageType::semaphore_init;
    uint32_t version = 1;
    uint64_t initial_value = 0;
    // Two FDs via SCM_RIGHTS: [frame_ready_fd, frame_consumed_fd]
};
```

### 7.6 Required Extensions

| Extension | Purpose |
|-----------|---------|
| `VK_KHR_timeline_semaphore` | Timeline semaphore semantics |
| `VK_KHR_external_semaphore` | Enable semaphore sharing |
| `VK_KHR_external_semaphore_fd` | Export/import via file descriptor |

---

## 8. Capture Modes

Goggles supports two capture modes with different tradeoffs:

### 8.1 Default Mode (`GOGGLES_CAPTURE=1`)

Real-time capture with display - the target application window remains visible.

**Flow:**
1. Layer intercepts `vkQueuePresentKHR`
2. Copies swapchain image → separate DMA-BUF export image
3. Sends via IPC with timeline semaphore sync
4. Target application presents to real display

**Characteristics:**
- GPU copy required on each frame
- Full timeline semaphore synchronization
- Target window displayed on screen
- Async worker thread for non-blocking capture

### 8.2 WSI Proxy Mode (`GOGGLES_CAPTURE=1 GOGGLES_WSI_PROXY=1`)

Headless capture - completely virtualizes the Window System Interface.

**Flow:**
1. Layer returns synthetic VkSurface/VkSwapchain handles
2. Application renders directly to DMA-BUF exportable images
3. No GPU copy - images sent directly via IPC
4. No real window created

**Characteristics:**
- Zero GPU copy overhead
- Simplified synchronization (FPS-limited acquire)
- No target window displayed
- Inline IPC (no async worker)

### 8.3 Comparison Table

| Aspect | Default Mode | WSI Proxy Mode |
|--------|--------------|----------------|
| **Activation** | `GOGGLES_CAPTURE=1` | `GOGGLES_CAPTURE=1 GOGGLES_WSI_PROXY=1` |
| **Window** | Real X11/Wayland displayed | No window (virtual) |
| **Swapchain** | Real driver images | DMA-BUF exportable from creation |
| **GPU Copy** | Required (swapchain → export) | None (direct export) |
| **Sync** | Timeline semaphores + async worker | Inline IPC, FPS-limited |
| **Use Case** | Real-time display + capture | Headless capture only |

### 8.4 WSI Proxy Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `GOGGLES_WIDTH` | 1920 | Virtual surface width |
| `GOGGLES_HEIGHT` | 1080 | Virtual surface height |
| `GOGGLES_FPS_LIMIT` | 60 | Frame rate limit at acquire |

---

## 9. References

- [Vulkan VK_EXT_image_drm_format_modifier](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_image_drm_format_modifier.html)
- [Vulkan VK_KHR_timeline_semaphore](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_timeline_semaphore.html)
- [Vulkan VK_KHR_external_semaphore](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_external_semaphore.html)
- [Linux DRM Format Modifiers](https://docs.kernel.org/gpu/drm-kms.html#format-modifiers)
- [DMA-BUF Sharing](https://docs.kernel.org/driver-api/dma-buf.html)
