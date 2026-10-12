// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace Vulkan {
// Track both sources even when hidden, so changing layouts cannot count old submissions as new.
class SourceFrameCounter {
public:
    std::uint64_t Update(const std::array<std::uint64_t, 2>& frames, bool top, bool bottom) {
        std::array<std::uint64_t, 2> delta{};
        for (unsigned screen = 0; screen < 2; ++screen) {
            if (frames[screen] >= previous[screen])
                delta[screen] = frames[screen] - previous[screen];
        }
        previous = frames;
        // A composite window gets one update when both screens submit in the same render tick.
        return std::max(top ? delta[0] : 0, bottom ? delta[1] : 0);
    }

private:
    std::array<std::uint64_t, 2> previous{};
};
} // namespace Vulkan
