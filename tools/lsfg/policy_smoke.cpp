// SPDX-License-Identifier: GPL-2.0-or-later
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "video_core/renderer_vulkan/lsfg_policy.h"

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
    std::cout << "15 LSFG cadence and dual-display policy checks passed\n";
}
