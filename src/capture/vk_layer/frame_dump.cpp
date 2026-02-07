#include "frame_dump.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <util/profiling.hpp>

namespace goggles::capture {

namespace {

struct Time {
    static constexpr uint64_t infinite = UINT64_MAX;
};

struct DumpResources {
    VkFence fence = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

auto is_supported_dump_format(VkFormat format, bool* is_bgra) -> bool {
    switch (format) {
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
        *is_bgra = true;
        return true;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        *is_bgra = false;
        return true;
    default:
        return false;
    }
}

auto sanitize_filename_component(std::string_view in) -> std::string {
    std::string out;
    out.reserve(in.size());

    for (char ch : in) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch) || ch == '_' || ch == '-' || ch == '.') {
            out.push_back(ch);
        } else {
            out.push_back('_');
        }
    }

    if (out.empty()) {
        out = "process";
    }
    return out;
}

auto get_process_name() -> std::string {
    std::array<char, 16> name{};
    if (prctl(PR_GET_NAME, name.data(), 0, 0, 0) != 0) {
        return "process";
    }

    name.back() = '\0';
    return sanitize_filename_component(std::string_view{name.data()});
}

auto mkdir_p(const std::string& path) -> bool {
    if (path.empty()) {
        return false;
    }

    std::string current;
    current.reserve(path.size());

    for (size_t i = 0; i < path.size(); ++i) {
        char ch = path[i];
        current.push_back(ch);

        bool is_separator = (ch == '/');
        bool is_last = (i + 1 == path.size());
        if (!is_separator && !is_last) {
            continue;
        }

        if (current.size() == 1 && current[0] == '/') {
            continue;
        }

        if (::mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
            return false;
        }
    }

    return true;
}

auto write_ppm_file(const std::string& path, const uint8_t* pixels_rgba, uint32_t width,
                    uint32_t height, bool is_bgra) -> bool {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }

    std::fprintf(f, "P6\n%u %u\n255\n", width, height);

    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t* row = pixels_rgba + static_cast<size_t>(y) * width * 4;
        for (uint32_t x = 0; x < width; ++x) {
            const uint8_t* px = row + static_cast<size_t>(x) * 4;
            uint8_t rgb[3] = {
                is_bgra ? px[2] : px[0],
                px[1],
                is_bgra ? px[0] : px[2],
            };
            std::fwrite(rgb, 1, sizeof(rgb), f);
        }
    }

    std::fclose(f);
    return true;
}

auto write_desc_file(const std::string& path, const DumpJob& job, std::string_view process_name)
    -> bool {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }

    std::fprintf(f, "process_name=%.*s\n", static_cast<int>(process_name.size()),
                 process_name.data());
    std::fprintf(f, "pid=%d\n", static_cast<int>(::getpid()));
    std::fprintf(f, "frame_number=%" PRIu64 "\n", job.frame_number);
    std::fprintf(f, "width=%u\n", job.width);
    std::fprintf(f, "height=%u\n", job.height);
    std::fprintf(f, "format=%d\n", static_cast<int>(job.format));
    std::fprintf(f, "stride=%u\n", job.src.stride);
    std::fprintf(f, "offset=%u\n", job.src.offset);
    std::fprintf(f, "modifier=%" PRIu64 "\n", job.src.modifier);

    std::fclose(f);
    return true;
}

struct MemoryTypeRequest {
    uint32_t type_bits = 0;
    VkMemoryPropertyFlags required = 0;
    VkMemoryPropertyFlags preferred = 0;
};

auto find_memory_type(const VkPhysicalDeviceMemoryProperties& mem_props,
                      const MemoryTypeRequest& req, bool* out_is_coherent) -> uint32_t {
    uint32_t fallback = UINT32_MAX;
    bool fallback_coherent = false;

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((req.type_bits & (1u << i)) == 0) {
            continue;
        }
        VkMemoryPropertyFlags flags = mem_props.memoryTypes[i].propertyFlags;
        if ((flags & req.required) != req.required) {
            continue;
        }

        bool is_coherent = (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        if ((flags & req.preferred) == req.preferred) {
            *out_is_coherent = is_coherent;
            return i;
        }

        if (fallback == UINT32_MAX) {
            fallback = i;
            fallback_coherent = is_coherent;
        }
    }

    if (fallback != UINT32_MAX) {
        *out_is_coherent = fallback_coherent;
    }
    return fallback;
}

auto calc_dump_size_bytes(uint32_t width, uint32_t height) -> VkDeviceSize {
    return static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4u;
}

void cleanup_dump_resources(const VkDeviceFuncs& funcs, VkDevice device, const DumpResources& res) {
    if (res.fence != VK_NULL_HANDLE) {
        funcs.DestroyFence(device, res.fence, nullptr);
    }
    if (res.cmd != VK_NULL_HANDLE && res.pool != VK_NULL_HANDLE) {
        funcs.FreeCommandBuffers(device, res.pool, 1, &res.cmd);
    }
    if (res.pool != VK_NULL_HANDLE) {
        funcs.DestroyCommandPool(device, res.pool, nullptr);
    }
    if (res.buffer != VK_NULL_HANDLE) {
        funcs.DestroyBuffer(device, res.buffer, nullptr);
    }
    if (res.memory != VK_NULL_HANDLE) {
        funcs.FreeMemory(device, res.memory, nullptr);
    }
}

auto create_dump_buffer(VkDeviceData* dev_data, VkDeviceSize size, DumpJob* out_job) -> bool {
    GOGGLES_PROFILE_FUNCTION();
    auto& funcs = dev_data->funcs;
    VkDevice device = dev_data->device;

    VkBufferCreateInfo buf_info{};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = size;
    buf_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    if (funcs.CreateBuffer(device, &buf_info, nullptr, &buffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements mem_reqs{};
    funcs.GetBufferMemoryRequirements(device, buffer, &mem_reqs);

    VkPhysicalDeviceMemoryProperties mem_props{};
    dev_data->inst_data->funcs.GetPhysicalDeviceMemoryProperties(dev_data->physical_device,
                                                                 &mem_props);

    MemoryTypeRequest req{};
    req.type_bits = mem_reqs.memoryTypeBits;
    req.required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    req.preferred = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    bool mem_coherent = true;
    uint32_t mem_type = find_memory_type(mem_props, req, &mem_coherent);
    if (mem_type == UINT32_MAX) {
        funcs.DestroyBuffer(device, buffer, nullptr);
        return false;
    }

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (funcs.AllocateMemory(device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        funcs.DestroyBuffer(device, buffer, nullptr);
        return false;
    }

    if (funcs.BindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS) {
        funcs.FreeMemory(device, memory, nullptr);
        funcs.DestroyBuffer(device, buffer, nullptr);
        return false;
    }

    out_job->buffer = buffer;
    out_job->memory = memory;
    out_job->memory_is_coherent = mem_coherent;
    return true;
}

auto create_dump_command_buffer(VkDeviceData* dev_data, VkCommandPool* out_pool,
                                VkCommandBuffer* out_cmd) -> bool {
    GOGGLES_PROFILE_FUNCTION();
    auto& funcs = dev_data->funcs;
    VkDevice device = dev_data->device;

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags =
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = dev_data->graphics_queue_family;

    VkCommandPool pool = VK_NULL_HANDLE;
    if (funcs.CreateCommandPool(device, &pool_info, nullptr, &pool) != VK_SUCCESS) {
        return false;
    }

    VkCommandBufferAllocateInfo cmd_alloc{};
    cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc.commandPool = pool;
    cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (funcs.AllocateCommandBuffers(device, &cmd_alloc, &cmd) != VK_SUCCESS) {
        funcs.DestroyCommandPool(device, pool, nullptr);
        return false;
    }

    *out_pool = pool;
    *out_cmd = cmd;
    return true;
}

auto create_dump_fence(const VkDeviceFuncs& funcs, VkDevice device, VkFence* out_fence) -> bool {
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    return funcs.CreateFence(device, &fence_info, nullptr, out_fence) == VK_SUCCESS;
}

void record_present_image_copy(const VkDeviceFuncs& funcs, VkCommandBuffer cmd, VkImage image,
                               uint32_t width, uint32_t height, VkBuffer buffer) {
    VkImageSubresourceRange subres{};
    subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subres.baseMipLevel = 0;
    subres.levelCount = 1;
    subres.baseArrayLayer = 0;
    subres.layerCount = 1;

    VkImageMemoryBarrier to_transfer{};
    to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_transfer.srcAccessMask = 0;
    to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    to_transfer.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image = image;
    to_transfer.subresourceRange = subres;

    funcs.CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &to_transfer);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    funcs.CmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1,
                               &region);

    VkImageMemoryBarrier to_present{};
    to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    to_present.dstAccessMask = 0;
    to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.image = image;
    to_present.subresourceRange = subres;

    funcs.CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &to_present);
}

void record_export_image_copy(const VkDeviceFuncs& funcs, VkCommandBuffer cmd, VkImage image,
                              uint32_t width, uint32_t height, VkBuffer buffer) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    funcs.CmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_GENERAL, buffer, 1, &region);
}

} // namespace

FrameDumper::FrameDumper() {
    parse_env_config();
}

void FrameDumper::parse_env_config() {
    GOGGLES_PROFILE_FUNCTION();
    enabled_ = false;
    mode_ = DumpFrameMode::ppm;
    dump_dir_ = "/tmp/goggles_dump";
    process_name_ = get_process_name();
    ranges_.clear();

    const char* dump_dir = std::getenv("GOGGLES_DUMP_DIR");
    if (dump_dir && dump_dir[0] != '\0') {
        dump_dir_ = dump_dir;
    }

    const char* mode = std::getenv("GOGGLES_DUMP_FRAME_MODE");
    if (mode && mode[0] != '\0') {
        std::string_view mode_view{mode};
        if (mode_view == "ppm" || mode_view == "PPM") {
            mode_ = DumpFrameMode::ppm;
        } else {
            mode_ = DumpFrameMode::ppm;
        }
    }

    const char* range = std::getenv("GOGGLES_DUMP_FRAME_RANGE");
    if (!range || range[0] == '\0') {
        enabled_ = false;
        return;
    }

    std::vector<DumpRange> ranges;
    std::string_view view{range};
    size_t pos = 0;
    while (pos < view.size()) {
        while (pos < view.size() &&
               (view[pos] == ',' || std::isspace(static_cast<unsigned char>(view[pos])))) {
            ++pos;
        }
        if (pos >= view.size()) {
            break;
        }

        auto parse_number = [&](uint64_t* out) -> bool {
            while (pos < view.size() && std::isspace(static_cast<unsigned char>(view[pos]))) {
                ++pos;
            }
            if (pos >= view.size() || !std::isdigit(static_cast<unsigned char>(view[pos]))) {
                return false;
            }
            uint64_t val = 0;
            while (pos < view.size() && std::isdigit(static_cast<unsigned char>(view[pos]))) {
                val = val * 10 + static_cast<uint64_t>(view[pos] - '0');
                ++pos;
            }
            *out = val;
            return true;
        };

        uint64_t begin = 0;
        if (!parse_number(&begin) || begin == 0) {
            while (pos < view.size() && view[pos] != ',') {
                ++pos;
            }
            continue;
        }

        while (pos < view.size() && std::isspace(static_cast<unsigned char>(view[pos]))) {
            ++pos;
        }

        uint64_t end = begin;
        if (pos < view.size() && view[pos] == '-') {
            ++pos;
            if (!parse_number(&end) || end == 0) {
                while (pos < view.size() && view[pos] != ',') {
                    ++pos;
                }
                continue;
            }
        }

        if (end < begin) {
            std::swap(begin, end);
        }
        ranges.push_back({begin, end});

        while (pos < view.size() && view[pos] != ',') {
            ++pos;
        }
    }

    if (ranges.empty()) {
        enabled_ = false;
        return;
    }

    std::sort(ranges.begin(), ranges.end(),
              [](const DumpRange& a, const DumpRange& b) { return a.begin < b.begin; });
    std::vector<DumpRange> merged;
    merged.reserve(ranges.size());
    for (const auto& r : ranges) {
        if (merged.empty()) {
            merged.push_back(r);
            continue;
        }
        auto& last = merged.back();
        if (r.begin <= last.end + 1) {
            last.end = std::max(last.end, r.end);
        } else {
            merged.push_back(r);
        }
    }

    ranges_ = std::move(merged);
    enabled_ = true;
}

auto FrameDumper::should_dump_frame(uint64_t frame_number) const -> bool {
    if (!enabled_ || ranges_.empty()) {
        return false;
    }

    size_t lo = 0;
    size_t hi = ranges_.size();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (frame_number < ranges_[mid].begin) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }

    if (lo == 0) {
        return false;
    }
    const DumpRange& r = ranges_[lo - 1];
    return frame_number >= r.begin && frame_number <= r.end;
}

auto FrameDumper::try_schedule_export_image_dump(VkQueue queue, VkDeviceData* dev_data,
                                                 VkImage image, uint32_t width, uint32_t height,
                                                 VkFormat format, uint64_t frame_number,
                                                 TimelineWait wait, const DumpSourceInfo& src)
    -> bool {
    GOGGLES_PROFILE_FUNCTION();
    if (!enabled_ || !should_dump_frame(frame_number)) {
        return false;
    }
    return schedule_dump_copy_timeline(queue, dev_data, image, width, height, format, frame_number,
                                       src, wait);
}

auto FrameDumper::try_schedule_present_image_dump(VkQueue queue, VkDeviceData* dev_data,
                                                  VkImage image, uint32_t width, uint32_t height,
                                                  VkFormat format, uint64_t frame_number,
                                                  const DumpSourceInfo& src, uint32_t wait_count,
                                                  const VkSemaphore* wait_semaphores) -> bool {
    GOGGLES_PROFILE_FUNCTION();
    if (!enabled_ || !should_dump_frame(frame_number)) {
        return false;
    }
    return schedule_dump_copy(queue, dev_data, image, width, height, format, frame_number, src,
                              wait_count, wait_semaphores);
}

auto FrameDumper::schedule_dump_copy(VkQueue queue, VkDeviceData* dev_data, VkImage image,
                                     uint32_t width, uint32_t height, VkFormat format,
                                     uint64_t frame_number, const DumpSourceInfo& src,
                                     uint32_t wait_count, const VkSemaphore* wait_semaphores)
    -> bool {
    GOGGLES_PROFILE_FUNCTION();
    if (!enabled_ || image == VK_NULL_HANDLE) {
        return false;
    }
    if (wait_count > 0 && wait_semaphores == nullptr) {
        return false;
    }
    constexpr size_t MAX_WAIT_SEMAPHORES = 64;
    if (wait_count > MAX_WAIT_SEMAPHORES) {
        return false;
    }

    bool is_bgra = false;
    if (!is_supported_dump_format(format, &is_bgra)) {
        return false;
    }

    VkDeviceSize size = calc_dump_size_bytes(width, height);
    if (size == 0) {
        return false;
    }

    VkDevice device = dev_data->device;
    auto& funcs = dev_data->funcs;

    DumpJob job{};
    job.device = device;
    job.funcs = funcs;
    job.frame_number = frame_number;
    job.width = width;
    job.height = height;
    job.format = format;
    job.src = src;
    job.is_bgra = is_bgra;

    if (!create_dump_buffer(dev_data, size, &job)) {
        return false;
    }
    if (!create_dump_command_buffer(dev_data, &job.pool, &job.cmd)) {
        cleanup_dump_resources(funcs, device,
                               DumpResources{.buffer = job.buffer, .memory = job.memory});
        return false;
    }
    if (!create_dump_fence(funcs, device, &job.fence)) {
        cleanup_dump_resources(
            funcs, device,
            DumpResources{
                .pool = job.pool, .cmd = job.cmd, .buffer = job.buffer, .memory = job.memory});
        return false;
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    funcs.BeginCommandBuffer(job.cmd, &begin_info);
    record_present_image_copy(funcs, job.cmd, image, width, height, job.buffer);
    funcs.EndCommandBuffer(job.cmd);

    std::array<VkPipelineStageFlags, MAX_WAIT_SEMAPHORES> wait_stage_mask{};
    wait_stage_mask.fill(VK_PIPELINE_STAGE_TRANSFER_BIT);
    const VkPipelineStageFlags* wait_stages = wait_count > 0 ? wait_stage_mask.data() : nullptr;
    const VkSemaphore* wait_sems = wait_count > 0 ? wait_semaphores : nullptr;

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = wait_count;
    submit.pWaitSemaphores = wait_sems;
    submit.pWaitDstStageMask = wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &job.cmd;

    const DumpResources res{.fence = job.fence,
                            .pool = job.pool,
                            .cmd = job.cmd,
                            .buffer = job.buffer,
                            .memory = job.memory};

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        if (queue_.size() >= queue_.capacity()) {
            // Queue is full; do not submit.
            // Cleanup is outside the lock to avoid blocking other producers on Vulkan teardown.
        } else if (funcs.QueueSubmit(queue, 1, &submit, job.fence) != VK_SUCCESS) {
            // Submit failed; nothing is in-flight.
        } else {
            // After a successful QueueSubmit the GPU may still be using job resources; do not
            // destroy them on any enqueue failure from this thread.
            return queue_.try_push(job);
        }
    }

    cleanup_dump_resources(funcs, device, res);
    return false;
}

auto FrameDumper::schedule_dump_copy_timeline(VkQueue queue, VkDeviceData* dev_data, VkImage image,
                                              uint32_t width, uint32_t height, VkFormat format,
                                              uint64_t frame_number, const DumpSourceInfo& src,
                                              TimelineWait wait) -> bool {
    GOGGLES_PROFILE_FUNCTION();
    if (!enabled_ || image == VK_NULL_HANDLE) {
        return false;
    }

    bool is_bgra = false;
    if (!is_supported_dump_format(format, &is_bgra)) {
        return false;
    }

    VkDeviceSize size = calc_dump_size_bytes(width, height);
    if (size == 0) {
        return false;
    }

    VkDevice device = dev_data->device;
    auto& funcs = dev_data->funcs;

    DumpJob job{};
    job.device = device;
    job.funcs = funcs;
    job.frame_number = frame_number;
    job.width = width;
    job.height = height;
    job.format = format;
    job.src = src;
    job.is_bgra = is_bgra;

    if (!create_dump_buffer(dev_data, size, &job)) {
        return false;
    }
    if (!create_dump_command_buffer(dev_data, &job.pool, &job.cmd)) {
        cleanup_dump_resources(funcs, device,
                               DumpResources{.buffer = job.buffer, .memory = job.memory});
        return false;
    }
    if (!create_dump_fence(funcs, device, &job.fence)) {
        cleanup_dump_resources(
            funcs, device,
            DumpResources{
                .pool = job.pool, .cmd = job.cmd, .buffer = job.buffer, .memory = job.memory});
        return false;
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    funcs.BeginCommandBuffer(job.cmd, &begin_info);
    record_export_image_copy(funcs, job.cmd, image, width, height, job.buffer);
    funcs.EndCommandBuffer(job.cmd);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkTimelineSemaphoreSubmitInfo timeline_submit{};
    timeline_submit.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline_submit.waitSemaphoreValueCount = 1;
    timeline_submit.pWaitSemaphoreValues = &wait.value;

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = &timeline_submit;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &wait.semaphore;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &job.cmd;

    const DumpResources res{.fence = job.fence,
                            .pool = job.pool,
                            .cmd = job.cmd,
                            .buffer = job.buffer,
                            .memory = job.memory};

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        if (queue_.size() >= queue_.capacity()) {
            // Queue is full; do not submit.
            // Cleanup is outside the lock to avoid blocking other producers on Vulkan teardown.
        } else if (funcs.QueueSubmit(queue, 1, &submit, job.fence) != VK_SUCCESS) {
            // Submit failed; nothing is in-flight.
        } else {
            // After a successful QueueSubmit the GPU may still be using job resources; do not
            // destroy them on any enqueue failure from this thread.
            return queue_.try_push(job);
        }
    }

    cleanup_dump_resources(funcs, device, res);
    return false;
}

void FrameDumper::drain_job(DumpJob& job) {
    GOGGLES_PROFILE_FUNCTION();
    if (job.fence != VK_NULL_HANDLE) {
        job.funcs.WaitForFences(job.device, 1, &job.fence, VK_TRUE, Time::infinite);
    }

    if (!mkdir_p(dump_dir_)) {
        cleanup_dump_resources(job.funcs, job.device,
                               DumpResources{.fence = job.fence,
                                             .pool = job.pool,
                                             .cmd = job.cmd,
                                             .buffer = job.buffer,
                                             .memory = job.memory});
        return;
    }

    std::string base = process_name_ + "_" + std::to_string(job.frame_number);
    std::string ppm_path = dump_dir_ + "/" + base + ".ppm";
    std::string desc_path = ppm_path + ".desc";

    void* mapped = nullptr;
    if (job.funcs.MapMemory(job.device, job.memory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS ||
        mapped == nullptr) {
        write_desc_file(desc_path, job, process_name_);
        cleanup_dump_resources(job.funcs, job.device,
                               DumpResources{.fence = job.fence,
                                             .pool = job.pool,
                                             .cmd = job.cmd,
                                             .buffer = job.buffer,
                                             .memory = job.memory});
        return;
    }

    if (!job.memory_is_coherent) {
        VkMappedMemoryRange range{};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = job.memory;
        range.offset = 0;
        range.size = VK_WHOLE_SIZE;
        job.funcs.InvalidateMappedMemoryRanges(job.device, 1, &range);
    }

    write_ppm_file(ppm_path, static_cast<const uint8_t*>(mapped), job.width, job.height,
                   job.is_bgra);
    write_desc_file(desc_path, job, process_name_);

    job.funcs.UnmapMemory(job.device, job.memory);

    cleanup_dump_resources(job.funcs, job.device,
                           DumpResources{.fence = job.fence,
                                         .pool = job.pool,
                                         .cmd = job.cmd,
                                         .buffer = job.buffer,
                                         .memory = job.memory});
}

void FrameDumper::drain() {
    GOGGLES_PROFILE_FUNCTION();
    while (!queue_.empty()) {
        auto opt = queue_.try_pop();
        if (!opt) {
            break;
        }
        DumpJob job = *opt;
        drain_job(job);
    }
}

} // namespace goggles::capture
