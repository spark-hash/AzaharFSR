// SPDX-License-Identifier: GPL-2.0-or-later
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "video_core/renderer_vulkan/lsfg_policy.h"
#include "video_core/renderer_vulkan/source_frame_counter.h"

int main() {
    const auto check = [](bool value) {
        if (!value) throw std::runtime_error("LSFG display policy failed");
    };
    using Vulkan::LsfgMultiplier;
    using Vulkan::LsfgSpacing;
    check(LsfgMultiplier(0, 120, 1.0 / 30) == 1);
    check(LsfgMultiplier(1, 120, 1.0 / 30) == 2);
    check(LsfgMultiplier(1, 120, 1.0 / 60) == 1);
    check(LsfgMultiplier(2, 120, 1.0 / 30) == 2);
    check(LsfgMultiplier(2, 120, 1.0 / 60) == 1);
    check(LsfgMultiplier(2, 60, 1.0 / 30) == 2);
    check(LsfgMultiplier(2, 60, 1.0 / 60) == 1);
    check(LsfgMultiplier(2, 90, 1.0 / 30) == 2);
    check(LsfgMultiplier(2, 144, 1.0 / 30) == 2);
    check(LsfgMultiplier(2, 59.94, 1.0 / 30) == 2);
    check(LsfgMultiplier(2, 119.88, 1.0 / 60) == 1);
    check(LsfgMultiplier(2, std::numeric_limits<double>::quiet_NaN(), 1.0 / 30) == 1);
    check(LsfgSpacing(2, 120, 0.028) >= 1.0 / 60);
    check(LsfgSpacing(2, 60, 0.028) >= 1.0 / 60);
    check(std::abs(LsfgSpacing(2, 120, 1.0 / 30) - 1.0 / 60) < 1e-9);
    Vulkan::SourceFrameCounter source;
    check(source.Update({60, 30}, false, true) == 30);
    check(source.Update({120, 30}, false, true) == 0);
    check(source.Update({120, 30}, true, false) == 0);
    check(source.Update({150, 60}, true, true) == 30);
    check(source.Update({0, 0}, true, true) == 0);
    std::cout << "20 LSFG cadence and per-screen submission checks passed\n";
}
