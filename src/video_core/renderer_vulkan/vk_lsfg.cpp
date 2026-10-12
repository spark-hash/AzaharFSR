// SPDX-License-Identifier: GPL-2.0-or-later
#include <fstream>
#include <stdexcept>
#include "common/logging/log.h"
#include "common/settings.h"
#include "video_core/renderer_vulkan/lsfg_assets.h"
#include "video_core/renderer_vulkan/lsfg_policy.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_lsfg.h"
#include "video_core/renderer_vulkan/vk_present_window.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

#include <lsfg_3_1p.hpp>
#include <vk_mem_alloc.h>

namespace Vulkan {
namespace {
// All library operations are serialized by the shared Scheduler::submit_mutex.
unsigned library_users{};
constexpr vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
std::vector<uint8_t> LoadShader(const std::string& path, const std::string& name) {
    std::ifstream stream(path + "/" + name + ".spv", std::ios::binary | std::ios::ate);
    const auto size = stream.tellg();
    if (size < 20 || size > 4 * 1024 * 1024 || size % 4) {
        throw std::runtime_error("Missing or invalid imported LSFG shader: " + name);
    }
    std::vector<uint8_t> bytes(static_cast<std::size_t>(size));
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), size)) {
        throw std::runtime_error("Unable to read LSFG shader: " + name);
    }
    return bytes;
}
} // namespace

Lsfg::Lsfg(const Instance& instance_, Scheduler& scheduler_, bool main_window_)
    : instance{instance_}, scheduler{scheduler_}, main_window{main_window_} {}
Lsfg::~Lsfg() {
    std::scoped_lock lock{scheduler.submit_mutex};
    Reset();
}
void Lsfg::Status(int state, double milliseconds) {
    SetLsfgStatus(state, milliseconds, main_window, multiplier);
}
void Lsfg::Reset() {
    instance.GetGraphicsQueue().waitIdle();
    if (context >= 0)
        LSFG_3_1P::deleteContext(context);
    if (initialized && --library_users == 0)
        LSFG_3_1P::finalize();
    initialized = false;
    context = -1;
    pool.reset();
    for (std::size_t index = 0; index < images.size(); ++index) {
        if (images[index])
            vmaDestroyImage(instance.GetAllocator(), images[index], allocations[index]);
        images[index] = nullptr;
        allocations[index] = nullptr;
    }
    images.clear();
    allocations.clear();
    history = false;
    slot = 0;
    previous = {};
    previous_emulated_us = 0;
}
void Lsfg::Create(u32 w, u32 h, const std::string& path, unsigned factor) {
    const auto physical = instance.GetPhysicalDevice();
    const auto features = physical.getFeatures();
    if (!instance.IsLsfgSupported() || !features.shaderStorageImageExtendedFormats ||
        !features.shaderStorageImageReadWithoutFormat ||
        !features.shaderStorageImageWriteWithoutFormat || w < 64 || h < 64 ||
        physical.getProperties().limits.maxComputeWorkGroupInvocations < 1024) {
        throw std::runtime_error("LSFG requires Vulkan 1.3, Vulkan memory model, storage images "
                                 "and 1024 compute invocations");
    }
    const auto format = physical.getFormatProperties(vk::Format::eR8G8B8A8Unorm);
    const auto required = vk::FormatFeatureFlagBits::eStorageImage |
                          vk::FormatFeatureFlagBits::eSampledImage |
                          vk::FormatFeatureFlagBits::eBlitSrc | vk::FormatFeatureFlagBits::eBlitDst;
    if ((format.optimalTilingFeatures & required) != required) {
        throw std::runtime_error("Device lacks LSFG RGBA8 format support");
    }
    width = w;
    height = h;
    asset_path = path;
    multiplier = factor;
    images.resize(factor + 1);
    allocations.resize(factor + 1);
    pool = instance.GetDevice().createCommandPoolUnique(
        vk::CommandPoolCreateInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                  .queueFamilyIndex = instance.GetGraphicsQueueFamilyIndex()});
    command = instance.GetDevice()
                  .allocateCommandBuffers(
                      vk::CommandBufferAllocateInfo{.commandPool = *pool,
                                                    .level = vk::CommandBufferLevel::ePrimary,
                                                    .commandBufferCount = 1})
                  .front();
    for (std::size_t index = 0; index < images.size(); ++index) {
        const vk::ImageCreateInfo info{
            .imageType = vk::ImageType::e2D,
            .format = vk::Format::eR8G8B8A8Unorm,
            .extent = {w, h, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
                     vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc};
        const auto raw_info = static_cast<VkImageCreateInfo>(info);
        VmaAllocationCreateInfo allocation{};
        allocation.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        VkImage raw_image{};
        if (vmaCreateImage(instance.GetAllocator(), &raw_info, &allocation, &raw_image,
                           &allocations[index], nullptr) != VK_SUCCESS)
            throw std::runtime_error("LSFG image allocation failed");
        images[index] = raw_image;
    }
    command.begin(
        vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    std::vector<vk::ImageMemoryBarrier> barriers(images.size());
    for (std::size_t index = 0; index < images.size(); ++index) {
        barriers[index] = vk::ImageMemoryBarrier{.dstAccessMask = vk::AccessFlagBits::eMemoryRead |
                                                                  vk::AccessFlagBits::eMemoryWrite,
                                                 .oldLayout = vk::ImageLayout::eUndefined,
                                                 .newLayout = vk::ImageLayout::eGeneral,
                                                 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                 .image = images[index],
                                                 .subresourceRange = range};
    }
    command.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                            vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barriers);
    const vk::ClearColorValue zero{std::array<float, 4>{}};
    for (const auto image : images)
        command.clearColorImage(image, vk::ImageLayout::eGeneral, zero, range);
    for (auto& barrier : barriers) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.oldLayout = vk::ImageLayout::eGeneral;
    }
    command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barriers);
    command.end();
    instance.GetGraphicsQueue().submit(
        vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &command}, {});
    instance.GetGraphicsQueue().waitIdle();
    if (library_users == 0) {
        try {
            LSFG_3_1P::initializeOnDevice(
                instance.GetInstance(), physical, instance.GetDevice(),
                instance.GetGraphicsQueueFamilyIndex(), instance.GetGraphicsQueue(),
                VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
                instance.IsNullDescriptorSupported(), 1.0f,
                [path](const std::string& name) { return LoadShader(path, name); }, 1);
        } catch (...) {
            LSFG_3_1P::finalize();
            throw;
        }
    }
    ++library_users;
    initialized = true;
    context = LSFG_3_1P::createContextFromImages(
        images[0], images[1], {images.begin() + 2, images.end()}, {w, h}, VK_FORMAT_R8G8B8A8_UNORM);
    LOG_INFO(Render_Vulkan, "LSFG 3.1 Performance {}x initialized on Azahar Vulkan device ({})",
             factor, main_window ? "main" : "secondary");
}
Lsfg::Result Lsfg::Prepare(Frame* frame) {
    Result result;
    const auto path = GetLsfgShaderDirectory();
    if (path.empty()) {
        Status(1);
        return result;
    }
    std::scoped_lock lock{scheduler.submit_mutex};
    if (asset_path != path) {
        failed = false;
        Reset();
        asset_path = path;
    }
    if (failed) {
        Status(5);
        return result;
    }
    try {
        const auto interval = frame->source_time - previous;
        const double refresh = GetLsfgDisplayRefreshRate(main_window);
        const double input_seconds = std::chrono::duration<double>(interval).count();
        const double native_seconds =
            previous_emulated_us > 0 && frame->source_emulated_us > previous_emulated_us
                ? (frame->source_emulated_us - previous_emulated_us) / 1e6
                : 0;
        const auto factor = LsfgMultiplier(GetLsfgMode(main_window), refresh, native_seconds);
        if (initialized &&
            (width != frame->width || height != frame->height || multiplier != factor))
            Reset();
        if (factor == 1 || (native_seconds > 0 && native_seconds < 0.014)) {
            if (initialized)
                Reset();
            previous = frame->source_time;
            previous_emulated_us = frame->source_emulated_us;
            Status(4);
            return result;
        }
        if (!initialized)
            Create(frame->width, frame->height, path, factor);
        instance.GetGraphicsQueue().waitIdle();
        const bool cadence_ok = interval > std::chrono::steady_clock::duration::zero() &&
                                interval <= std::chrono::milliseconds(250) && native_seconds > 0;
        const bool first_input = previous == std::chrono::steady_clock::time_point{};
        if (!cadence_ok) {
            if (history) {
                LSFG_3_1P::deleteContext(context);
                context = -1;
                context = LSFG_3_1P::createContextFromImages(
                    images[0], images[1], {images.begin() + 2, images.end()}, {width, height},
                    VK_FORMAT_R8G8B8A8_UNORM);
            }
            history = false;
            slot = 0;
        }
        command.reset();
        command.begin(
            vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        const std::array barriers{
            vk::ImageMemoryBarrier{.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                                   .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                                   .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
                                   .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                                   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                   .image = frame->image,
                                   .subresourceRange = range},
            vk::ImageMemoryBarrier{.srcAccessMask = vk::AccessFlagBits::eMemoryRead |
                                                    vk::AccessFlagBits::eMemoryWrite,
                                   .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                                   .oldLayout = vk::ImageLayout::eGeneral,
                                   .newLayout = vk::ImageLayout::eGeneral,
                                   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                   .image = images[slot],
                                   .subresourceRange = range}};
        command.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                                vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barriers);
        const vk::ImageSubresourceLayers layers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        const std::array offsets{vk::Offset3D{0, 0, 0}, vk::Offset3D{static_cast<int>(width),
                                                                     static_cast<int>(height), 1}};
        command.blitImage(frame->image, vk::ImageLayout::eTransferSrcOptimal, images[slot],
                          vk::ImageLayout::eGeneral,
                          vk::ImageBlit{.srcSubresource = layers,
                                        .srcOffsets = offsets,
                                        .dstSubresource = layers,
                                        .dstOffsets = offsets},
                          vk::Filter::eNearest);
        const vk::ImageMemoryBarrier copied{.srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                                            .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                                            .oldLayout = vk::ImageLayout::eGeneral,
                                            .newLayout = vk::ImageLayout::eGeneral,
                                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                            .image = images[slot],
                                            .subresourceRange = range};
        command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, copied);
        command.end();
        const vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eTransfer;
        instance.GetGraphicsQueue().submit(vk::SubmitInfo{.waitSemaphoreCount = 1,
                                                          .pWaitSemaphores = &frame->render_ready,
                                                          .pWaitDstStageMask = &stage,
                                                          .commandBufferCount = 1,
                                                          .pCommandBuffers = &command},
                                           {});
        result.consumed = true;
        instance.GetGraphicsQueue().waitIdle();
        result.real = images[slot];
        if (first_input || cadence_ok) {
            const auto start = std::chrono::steady_clock::now();
            LSFG_3_1P::presentContext(context, -1, {});
            LSFG_3_1P::waitIdle();
            const auto elapsed = std::chrono::steady_clock::now() - start;
            if (history && cadence_ok) {
                result.generated.assign(images.begin() + 2, images.end());
                result.spacing = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(LsfgSpacing(factor, 60.0, input_seconds)));
                Status(3, std::chrono::duration<double, std::milli>(elapsed).count());
            } else {
                Status(2);
            }
        } else {
            Status(first_input || cadence_ok ? 2 : 4);
        }
        previous = frame->source_time;
        previous_emulated_us = frame->source_emulated_us;
        history = first_input || cadence_ok;
        slot = history ? slot ^ 1 : 0;
        return result;
    } catch (const std::exception& error) {
        LOG_ERROR(Render_Vulkan, "LSFG failed; reverting to real frames: {}", error.what());
        Reset();
        failed = true;
        result.real = nullptr;
        result.generated.clear();
        Status(5);
        return result;
    }
}
} // namespace Vulkan
