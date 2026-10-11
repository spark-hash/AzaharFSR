// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include "common/math_util.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
VK_DEFINE_HANDLE(VmaAllocation)

namespace Vulkan {
class Instance;
class Scheduler;
class DescriptorUpdateQueue;

// Spatial FXAA or SMAA 1x, independently for each guest screen/eye.
class AntiAliasing final {
public:
    AntiAliasing(const Instance& instance, Scheduler& scheduler, DescriptorUpdateQueue& updates);
    ~AntiAliasing();
    vk::ImageView Apply(u32 slot, vk::ImageView input, Common::Rectangle<float> crop, u32 width,
                        u32 height, u32 mode);
    bool Available() const {
        return available;
    }

private:
    struct Image {
        vk::Image image{};
        vk::ImageView view{};
        VmaAllocation allocation{};
    };
    struct Target {
        std::array<Image, 4> images{};
        u32 width{}, height{};
    };
    void Destroy(Target& target);
    bool Resize(Target& target, u32 width, u32 height);
    bool UploadLookup(Image& image, const unsigned char* data, u32 width, u32 height, u32 channels);
    void DestroyImage(Image& image);
    std::array<Image, 2> lookups{};
    const Instance& instance;
    Scheduler& scheduler;
    DescriptorUpdateQueue& updates;
    DescriptorHeap heap;
    vk::UniquePipelineLayout layout;
    vk::UniqueSampler sampler;
    std::array<vk::UniquePipeline, 5> pipelines;
    std::array<Target, 3> targets{};
    u32 active_mode{};
    bool available{};
};
} // namespace Vulkan
