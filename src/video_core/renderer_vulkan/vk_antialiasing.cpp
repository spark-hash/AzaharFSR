// SPDX-License-Identifier: GPL-2.0-or-later
#include <string>
#include "common/logging/log.h"
#include <cstring>
#include <vector>
#include "../../../externals/smaa/AreaTex.h"
#include "../../../externals/smaa/SearchTex.h"
#include "video_core/host_shaders/SMAA.hlsl.h"
#include "video_core/host_shaders/Fxaa3_11.h.h"
#include "video_core/host_shaders/aa_copy_comp.h"
#include "video_core/host_shaders/fxaa_comp.h"
#include "video_core/host_shaders/smaa_edges_comp.h"
#include "video_core/host_shaders/smaa_weights_comp.h"
#include "video_core/host_shaders/smaa_blend_comp.h"
#include "video_core/renderer_vulkan/vk_descriptor_update_queue.h"
#include "video_core/renderer_vulkan/vk_antialiasing.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include <vk_mem_alloc.h>

namespace Vulkan {
namespace {
constexpr std::array bindings = {
    vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eCombinedImageSampler, 1,
                                   vk::ShaderStageFlagBits::eCompute},
    vk::DescriptorSetLayoutBinding{1, vk::DescriptorType::eStorageImage, 1,
                                   vk::ShaderStageFlagBits::eCompute},
    vk::DescriptorSetLayoutBinding{2, vk::DescriptorType::eCombinedImageSampler, 1,
                                   vk::ShaderStageFlagBits::eCompute},
    vk::DescriptorSetLayoutBinding{3, vk::DescriptorType::eCombinedImageSampler, 1,
                                   vk::ShaderStageFlagBits::eCompute},
    vk::DescriptorSetLayoutBinding{4, vk::DescriptorType::eCombinedImageSampler, 1,
                                   vk::ShaderStageFlagBits::eCompute},
};
struct Parameters {
    std::array<float, 4> crop;
    std::array<float, 2> output_size;
    float reserved;
};
constexpr vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
std::string Shader(std::string_view source) {
    std::string code{source};
    if (const auto pos = code.find("// FXAA\n"); pos != std::string::npos)
        code.replace(pos, 8, std::string{HostShaders::FXAA3_11_H} + "\n");
    if (const auto pos = code.find("// SMAA\n"); pos != std::string::npos) {
        std::string library{HostShaders::SMAA_HLSL};
        // Compute shaders cannot discard. Returning zero also clears old edge pixels.
        for (size_t p = 0; (p = library.find("discard;", p)) != std::string::npos;)
            library.replace(p, 8, "return float2(0.0);");
        // Compute has no fragment derivatives: use explicit level zero sampling.
        const std::array replacements = {
            std::pair{"texture(tex, coord)", "textureLod(tex, coord, 0.0)"},
            std::pair{"textureOffset(tex, coord, offset)",
                      "textureLodOffset(tex, coord, 0.0, offset)"}};
        for (const auto& [from, to] : replacements)
            for (size_t p = 0; (p = library.find(from, p)) != std::string::npos;
                 p += std::strlen(to))
                library.replace(p, std::strlen(from), to);
        code.replace(pos, 8, library + "\n");
    }
    return code;
}
} // namespace
AntiAliasing::AntiAliasing(const Instance& instance_, Scheduler& scheduler_,
                           DescriptorUpdateQueue& updates_)
    : instance{instance_}, scheduler{scheduler_}, updates{updates_},
      heap{instance, scheduler.GetMasterSemaphore(), bindings, 64} {
    const auto queues = instance.GetPhysicalDevice().getQueueFamilyProperties();
    if (!(queues[instance.GetGraphicsQueueFamilyIndex()].queueFlags &
          vk::QueueFlagBits::eCompute)) {
        LOG_WARNING(Render_Vulkan,
                    "Anti-aliasing unavailable: presentation queue lacks compute support");
        return;
    }
    const auto device = instance.GetDevice();
    const auto features = instance.GetPhysicalDevice()
                              .getFormatProperties(vk::Format::eR8G8B8A8Unorm)
                              .optimalTilingFeatures;
    const auto required = vk::FormatFeatureFlagBits::eStorageImage |
                          vk::FormatFeatureFlagBits::eSampledImage |
                          vk::FormatFeatureFlagBits::eSampledImageFilterLinear;
    if ((features & required) != required) {
        LOG_WARNING(Render_Vulkan,
                    "Anti-aliasing unavailable: RGBA8 storage/sample format unsupported");
        return;
    }
    const vk::PushConstantRange push{vk::ShaderStageFlagBits::eCompute, 0, sizeof(Parameters)};
    const auto descriptor_layout = heap.Layout();
    layout = device.createPipelineLayoutUnique({.setLayoutCount = 1,
                                                .pSetLayouts = &descriptor_layout,
                                                .pushConstantRangeCount = 1,
                                                .pPushConstantRanges = &push});
    sampler = device.createSamplerUnique({.magFilter = vk::Filter::eLinear,
                                          .minFilter = vk::Filter::eLinear,
                                          .addressModeU = vk::SamplerAddressMode::eClampToEdge,
                                          .addressModeV = vk::SamplerAddressMode::eClampToEdge,
                                          .addressModeW = vk::SamplerAddressMode::eClampToEdge});
    const std::array sources{HostShaders::AA_COPY_COMP, HostShaders::FXAA_COMP,
                             HostShaders::SMAA_EDGES_COMP, HostShaders::SMAA_WEIGHTS_COMP,
                             HostShaders::SMAA_BLEND_COMP};
    for (u32 i = 0; i < sources.size(); ++i) {
        const auto spv = CompileGLSL(Shader(sources[i]), vk::ShaderStageFlagBits::eCompute);
        if (spv.empty()) {
            LOG_ERROR(Render_Vulkan,
                      "Anti-aliasing shader compilation failed; using normal presentation");
            return;
        }
        const auto module = CompileSPV(spv, device);
        const vk::ComputePipelineCreateInfo info{
            .stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                      .module = module,
                      .pName = "main"},
            .layout = *layout};
        pipelines[i] = device.createComputePipelineUnique({}, info).value;
        device.destroyShaderModule(module);
    }
    if (!UploadLookup(lookups[0], areaTexBytes, AREATEX_WIDTH, AREATEX_HEIGHT, 2) ||
        !UploadLookup(lookups[1], searchTexBytes, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1)) {
        LOG_WARNING(Render_Vulkan, "Anti-aliasing lookup allocation failed");
        return;
    }
    available = true;
    LOG_INFO(Render_Vulkan, "FXAA 3.11 and SMAA 1x High initialized");
}
AntiAliasing::~AntiAliasing() {
    scheduler.Finish();
    for (auto& target : targets)
        Destroy(target);
    for (auto& image : lookups)
        DestroyImage(image);
}
void AntiAliasing::Destroy(Target& target) {
    for (auto& image : target.images)
        DestroyImage(image);
    target.width = target.height = 0;
}
bool AntiAliasing::Resize(Target& target, u32 width, u32 height) {
    const auto limit = instance.GetPhysicalDevice().getProperties().limits.maxImageDimension2D;
    if (width == 0 || height == 0 || width > limit || height > limit)
        return false;
    if (target.width == width && target.height == height)
        return true;
    scheduler.Finish();
    Destroy(target);
    for (auto& image : target.images) {
        const vk::ImageCreateInfo info{.imageType = vk::ImageType::e2D,
                                       .format = vk::Format::eR8G8B8A8Unorm,
                                       .extent = {width, height, 1},
                                       .mipLevels = 1,
                                       .arrayLayers = 1,
                                       .samples = vk::SampleCountFlagBits::e1,
                                       .usage = vk::ImageUsageFlagBits::eStorage |
                                                vk::ImageUsageFlagBits::eSampled};
        const VmaAllocationCreateInfo alloc{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
        const auto raw = static_cast<VkImageCreateInfo>(info);
        VkImage handle{};
        if (vmaCreateImage(instance.GetAllocator(), &raw, &alloc, &handle, &image.allocation,
                           nullptr) != VK_SUCCESS) {
            // Earlier images may already have a queued layout transition.
            scheduler.Finish();
            Destroy(target);
            LOG_WARNING(Render_Vulkan,
                        "Anti-aliasing allocation failed; using normal presentation");
            return false;
        }
        image.image = handle;
        image.view = instance.GetDevice().createImageView({.image = image.image,
                                                           .viewType = vk::ImageViewType::e2D,
                                                           .format = vk::Format::eR8G8B8A8Unorm,
                                                           .subresourceRange = range});
        scheduler.Record([handle = image.image](vk::CommandBuffer cmd) {
            const vk::ImageMemoryBarrier barrier{.dstAccessMask = vk::AccessFlagBits::eShaderWrite,
                                                 .oldLayout = vk::ImageLayout::eUndefined,
                                                 .newLayout = vk::ImageLayout::eGeneral,
                                                 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                 .image = handle,
                                                 .subresourceRange = range};
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, barrier);
        });
    }
    target.width = width;
    target.height = height;
    return true;
}
void AntiAliasing::DestroyImage(Image& image) {
    if (image.view)
        instance.GetDevice().destroyImageView(image.view);
    if (image.image)
        vmaDestroyImage(instance.GetAllocator(), image.image, image.allocation);
    image = {};
}
bool AntiAliasing::UploadLookup(Image& image, const unsigned char* data, u32 width, u32 height,
                                u32 channels) {
    std::vector<unsigned char> rgba(width * height * 4, 0);
    for (size_t p = 0; p < width * height; ++p)
        for (u32 channel = 0; channel < channels; ++channel)
            rgba[p * 4 + channel] = data[p * channels + channel];
    const vk::ImageCreateInfo info{.imageType = vk::ImageType::e2D,
                                   .format = vk::Format::eR8G8B8A8Unorm,
                                   .extent = {width, height, 1},
                                   .mipLevels = 1,
                                   .arrayLayers = 1,
                                   .samples = vk::SampleCountFlagBits::e1,
                                   .usage = vk::ImageUsageFlagBits::eTransferDst |
                                            vk::ImageUsageFlagBits::eSampled};
    const auto raw = static_cast<VkImageCreateInfo>(info);
    const VmaAllocationCreateInfo alloc{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    VkImage handle{};
    if (vmaCreateImage(instance.GetAllocator(), &raw, &alloc, &handle, &image.allocation,
                       nullptr) != VK_SUCCESS)
        return false;
    image.image = handle;
    image.view = instance.GetDevice().createImageView({.image = image.image,
                                                       .viewType = vk::ImageViewType::e2D,
                                                       .format = vk::Format::eR8G8B8A8Unorm,
                                                       .subresourceRange = range});
    const VkBufferCreateInfo buffer_info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                         .size = rgba.size(),
                                         .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
    const VmaAllocationCreateInfo staging_alloc{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO};
    VkBuffer buffer{};
    VmaAllocation allocation{};
    VmaAllocationInfo mapped{};
    if (vmaCreateBuffer(instance.GetAllocator(), &buffer_info, &staging_alloc, &buffer, &allocation,
                        &mapped) != VK_SUCCESS)
        return false;
    std::memcpy(mapped.pMappedData, rgba.data(), rgba.size());
    vmaFlushAllocation(instance.GetAllocator(), allocation, 0, rgba.size());
    scheduler.Record([handle = image.image, buffer, width, height](vk::CommandBuffer cmd) {
        const vk::ImageMemoryBarrier before{.dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                                            .oldLayout = vk::ImageLayout::eUndefined,
                                            .newLayout = vk::ImageLayout::eGeneral,
                                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                            .image = handle,
                                            .subresourceRange = range};
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                            vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, before);
        const vk::BufferImageCopy copy{
            .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            .imageExtent = {width, height, 1}};
        cmd.copyBufferToImage(buffer, handle, vk::ImageLayout::eGeneral, copy);
        const vk::ImageMemoryBarrier after{.srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                                           .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                                           .oldLayout = vk::ImageLayout::eGeneral,
                                           .newLayout = vk::ImageLayout::eGeneral,
                                           .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                           .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                           .image = handle,
                                           .subresourceRange = range};
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, after);
    });
    scheduler.Finish();
    vmaDestroyBuffer(instance.GetAllocator(), buffer, allocation);
    return true;
}
vk::ImageView AntiAliasing::Apply(u32 slot, vk::ImageView input, Common::Rectangle<float> crop,
                                  u32 width, u32 height, u32 mode) {
    auto& target = targets.at(slot);
    if (!available || (mode != 1 && mode != 2) || crop.right <= crop.left ||
        crop.bottom <= crop.top || !Resize(target, width, height))
        return {};
    if (active_mode != mode) {
        active_mode = mode;
        LOG_INFO(Render_Vulkan, "Presentation anti-aliasing: {}",
                 mode == 1 ? "FXAA" : "SMAA 1x High");
    }
    // Normalize the cropped guest framebuffer before sampling neighboring pixels.
    // 0 = copy, 1 = FXAA, 2/3/4 = SMAA edges/weights/neighborhood blend.
    const std::array<u32, 4> passes{0, mode == 1 ? 1u : 2u, 3, 4};
    const u32 pass_count = mode == 1 ? 2 : 4;
    for (u32 index = 0; index < pass_count; ++index) {
        const u32 pass = passes[index];
        const auto source = pass == 0   ? input
                            : pass == 3 ? target.images[1].view
                                        : target.images[0].view;
        const auto set = heap.Commit();
        updates.AddImageSampler(set, 0, 0, source, *sampler);
        updates.AddStorageImage(set, 1, target.images[index].view);
        updates.AddImageSampler(set, 2, 0, pass == 4 ? target.images[2].view : source, *sampler);
        updates.AddImageSampler(set, 3, 0, lookups[0].view, *sampler);
        updates.AddImageSampler(set, 4, 0, lookups[1].view, *sampler);
        const Parameters params{{crop.left, crop.top, crop.right, crop.bottom},
                                {static_cast<float>(width), static_cast<float>(height)},
                                0};
        scheduler.Record([pipeline = *pipelines[pass], pipeline_layout = *layout, set, params,
                          width, height](vk::CommandBuffer cmd) {
            const vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eMemoryWrite |
                                                             vk::AccessFlagBits::eMemoryRead,
                                            .dstAccessMask = vk::AccessFlagBits::eShaderRead |
                                                             vk::AccessFlagBits::eShaderWrite};
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                                vk::PipelineStageFlagBits::eComputeShader, {}, barrier, {}, {});
            cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, set, {});
            cmd.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(params),
                              &params);
            cmd.dispatch((width + 7) / 8, (height + 7) / 8, 1);
        });
    }
    scheduler.Record([](vk::CommandBuffer cmd) {
        const vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                                        .dstAccessMask = vk::AccessFlagBits::eShaderRead};
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                            vk::PipelineStageFlagBits::eFragmentShader, {}, barrier, {}, {});
    });
    return target.images[pass_count - 1].view;
}
} // namespace Vulkan
