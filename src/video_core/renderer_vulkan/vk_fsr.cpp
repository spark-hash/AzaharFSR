// SPDX-License-Identifier: GPL-2.0-or-later
#include <string>
#include "common/logging/log.h"
#include "video_core/host_shaders/ffx_a.h"
#include "video_core/host_shaders/ffx_fsr1.h"
#include "video_core/host_shaders/fsr_easu_comp.h"
#include "video_core/host_shaders/fsr_rcas_comp.h"
#include "video_core/renderer_vulkan/vk_descriptor_update_queue.h"
#include "video_core/renderer_vulkan/vk_fsr.h"
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
};
struct Parameters {
    std::array<float, 4> crop;
    std::array<float, 2> output_size;
    float sharpness;
};
constexpr vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
std::string Shader(std::string_view source) {
    std::string code{source};
    code.replace(code.find("// FFX_A"), 8, HostShaders::FFX_A_H);
    code.replace(code.find("// FFX_FSR1"), 11, HostShaders::FFX_FSR1_H);
    return code;
}
} // namespace
Fsr::Fsr(const Instance& instance_, Scheduler& scheduler_, DescriptorUpdateQueue& updates_)
    : instance{instance_}, scheduler{scheduler_}, updates{updates_},
      heap{instance, scheduler.GetMasterSemaphore(), bindings, 64} {
    const auto queues = instance.GetPhysicalDevice().getQueueFamilyProperties();
    if (!(queues[instance.GetGraphicsQueueFamilyIndex()].queueFlags & vk::QueueFlagBits::eCompute)) {
        LOG_WARNING(Render_Vulkan, "FSR 1 unavailable: presentation queue lacks compute support");
        return;
    }
    const auto device = instance.GetDevice();
    const auto features = instance.GetPhysicalDevice()
                              .getFormatProperties(vk::Format::eR8G8B8A8Unorm)
                              .optimalTilingFeatures;
    const auto required =
        vk::FormatFeatureFlagBits::eStorageImage | vk::FormatFeatureFlagBits::eSampledImage;
    if ((features & required) != required) {
        LOG_WARNING(Render_Vulkan, "FSR 1 unavailable: RGBA8 storage/sample format unsupported");
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
    const std::array sources{HostShaders::FSR_EASU_COMP, HostShaders::FSR_RCAS_COMP};
    for (u32 i = 0; i < 2; ++i) {
        const auto spv = CompileGLSL(Shader(sources[i]), vk::ShaderStageFlagBits::eCompute);
        if (spv.empty()) {
            LOG_ERROR(Render_Vulkan, "FSR 1 shader compilation failed; using normal presentation");
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
    available = true;
    LOG_INFO(Render_Vulkan, "FSR 1 EASU and RCAS initialized (FP32)");
}
Fsr::~Fsr() {
    scheduler.Finish();
    for (auto& target : targets)
        Destroy(target);
}
void Fsr::Destroy(Target& target) {
    for (auto& image : target.images) {
        if (image.view)
            instance.GetDevice().destroyImageView(image.view);
        if (image.image)
            vmaDestroyImage(instance.GetAllocator(), image.image, image.allocation);
        image = {};
    }
    target.width = target.height = 0;
}
bool Fsr::Resize(Target& target, u32 width, u32 height) {
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
            LOG_WARNING(Render_Vulkan, "FSR 1 allocation failed; using normal presentation");
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
vk::ImageView Fsr::Apply(u32 slot, vk::ImageView input, Common::Rectangle<float> crop, u32 width,
                         u32 height, float strength) {
    auto& target = targets.at(slot);
    if (!available || crop.right <= crop.left || crop.bottom <= crop.top ||
        !Resize(target, width, height))
        return {};
    const Parameters params{{crop.left, crop.top, crop.right, crop.bottom},
                            {static_cast<float>(width), static_cast<float>(height)},
                            2.0f * (1.0f - strength)};
    // Zero disables RCAS while retaining EASU upscaling.
    const u32 pass_count = strength > 0.0f ? 2 : 1;
    for (u32 pass = 0; pass < pass_count; ++pass) {
        const auto set = heap.Commit();
        updates.AddImageSampler(set, 0, 0, pass == 0 ? input : target.images[0].view, *sampler);
        updates.AddStorageImage(set, 1, target.images[pass].view);
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
