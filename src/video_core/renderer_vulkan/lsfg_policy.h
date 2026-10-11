// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>

namespace Vulkan {
// Integer interpolation ratios keep one history pair and avoid fractional frame cadence.
// input_interval uses emulated time. Refresh rate only detects an inactive display;
// an Android display-mode switch must not change the requested interpolation ratio.
inline unsigned LsfgMultiplier(unsigned mode, double refresh_rate, double input_interval) {
    if (mode == 0 || !std::isfinite(refresh_rate) || refresh_rate <= 0)
        return 1;
    const double ceiling = 60.0;
    const double input_rate = input_interval > 0 && input_interval < 0.025 ? 60.0 : 30.0;
    return std::clamp(static_cast<unsigned>((ceiling + 0.5) / input_rate), 1u, 2u);
}
inline double LsfgSpacing(unsigned multiplier, double refresh_rate, double input_interval) {
    const double ceiling = std::clamp(refresh_rate, 1.0, 60.0);
    return std::max(input_interval / std::max(multiplier, 1u), 1.0 / ceiling);
}
} // namespace Vulkan
