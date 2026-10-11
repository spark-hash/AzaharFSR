// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <chrono>
#include <string>
#include "video_core/renderer_vulkan/vk_common.h"
VK_DEFINE_HANDLE(VmaAllocation)

namespace Vulkan {
class Instance;
class Scheduler;
struct Frame;
class Lsfg {
public:
    struct Result {
        bool consumed{};
        vk::Image real{};
        vk::Image generated{};
        std::chrono::steady_clock::duration spacing{};
    };
    Lsfg(const Instance& instance, Scheduler& scheduler);
    ~Lsfg();
    Result Prepare(Frame* frame);

private:
    void Reset(); // Caller holds Scheduler::submit_mutex.
    void Create(u32 width, u32 height, const std::string& shaders);
    const Instance& instance;
    Scheduler& scheduler;
    std::array<vk::Image, 3> images{};
    std::array<VmaAllocation, 3> allocations{};
    vk::UniqueCommandPool pool;
    vk::CommandBuffer command;
    u32 width{}, height{}, slot{0};
    int context{-1};
    bool initialized{}, history{}, failed{};
    std::string asset_path;
    std::chrono::steady_clock::time_point previous{};
};
} // namespace Vulkan
