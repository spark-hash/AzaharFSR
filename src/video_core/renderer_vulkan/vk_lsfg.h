// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <chrono>
#include <string>
#include <vector>
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
        std::vector<vk::Image> generated;
        std::chrono::steady_clock::duration spacing{};
    };
    Lsfg(const Instance& instance, Scheduler& scheduler, bool main_window);
    ~Lsfg();
    Result Prepare(Frame* frame);

private:
    void Status(int state, double milliseconds = 0);
    void Reset(); // Caller holds Scheduler::submit_mutex.
    void Create(u32 width, u32 height, const std::string& shaders, unsigned multiplier);
    const Instance& instance;
    Scheduler& scheduler;
    std::vector<vk::Image> images;
    std::vector<VmaAllocation> allocations;
    vk::UniqueCommandPool pool;
    vk::CommandBuffer command;
    u32 width{}, height{}, slot{0};
    unsigned multiplier{2};
    int context{-1};
    bool initialized{}, history{}, failed{};
    bool main_window;
    std::string asset_path;
    std::chrono::steady_clock::time_point previous{};
    u64 previous_emulated_us{};
};
} // namespace Vulkan
