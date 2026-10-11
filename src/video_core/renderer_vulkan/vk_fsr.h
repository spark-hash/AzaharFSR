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

// Spatial EASU followed by RCAS, independently for each guest screen/eye.
class Fsr final {
public:
    Fsr(const Instance& instance, Scheduler& scheduler, DescriptorUpdateQueue& updates,
        bool anime4k = false);
    ~Fsr();
    vk::ImageView Apply(u32 slot, vk::ImageView input, Common::Rectangle<float> crop, u32 width,
                        u32 height, float strength);
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
        std::array<Image, 3> images{};
        u32 width{}, height{};
    };
    void Destroy(Target& target);
    bool Resize(Target& target, u32 width, u32 height);
    const Instance& instance;
    Scheduler& scheduler;
    DescriptorUpdateQueue& updates;
    DescriptorHeap heap;
    vk::UniquePipelineLayout layout;
    vk::UniqueSampler sampler;
    std::array<vk::UniquePipeline, 3> pipelines;
    std::array<Target, 3> targets{};
    bool available{};
    const bool anime4k;
};
} // namespace Vulkan
